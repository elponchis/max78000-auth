from flask import Flask, jsonify, request
from db import init_db, add_user, get_user, get_all_users, \
               update_lock, increment_fail, reset_fail, add_log, get_logs, DB_PATH
from uart import uart_a, uart_b, uart

# voxsv_demo의 manage_db.py에서 화자 등록 함수 import
import sys as _sys
_VOXSV_DB_GEN = '/home/pi/max78000-auth/synthesis/CNN/voxsv_demo/db_gen'
_SPEAKER_DB_H = '/home/pi/max78000-auth/synthesis/CNN/voxsv_demo/include/speaker_db.h'
_BOARD_B_PORT = '/dev/serial/by-id/usb-ARM_DAPLink_CMSIS-DAP_042317022187ca1b00000000000000000000000097969906-if01'
if _VOXSV_DB_GEN not in _sys.path:
    _sys.path.insert(0, _VOXSV_DB_GEN)
from manage_db import (recv_one, average_embeddings, write_header,
                       load_header, open_serial)

# 화자 등록 임시 저장소: {user_name: [emb_array, ...]}
_speaker_pending = {}

import threading as _threading
import numpy as _np

_SPEAKER_THRESHOLD = 0.65  # cosine similarity 임계값 (q7 정규화 임베딩)

def _board_b_authenticate_async():
    """보드B에 'G' 보내고 EMBED 받아서 speaker_db의 Jisoo와 비교, AUTH 결과 송신"""
    try:
        from manage_db import recv_one, open_serial, load_header
        ser = open_serial(_BOARD_B_PORT, 115200)
        if ser is None:
            print('[SPK_AUTH] board B open failed')
            uart_a.send('AUTH_FAIL')
            auth_state['status'] = 'fail'
            return
        try:
            emb, peak, rms = recv_one(ser, timeout=30)
        finally:
            ser.close()
        if emb is None:
            print(f'[SPK_AUTH] timeout (peak={peak}, rms={rms})')
            uart_a.send('AUTH_FAIL')
            auth_state['status'] = 'fail'
            return
        # speaker_db 로드 후 Jisoo 비교
        entries = load_header(_SPEAKER_DB_H)
        target = None
        for n, e in entries:
            if n == 'Jisoo':
                target = e
                break
        if target is None:
            print('[SPK_AUTH] Jisoo not in DB')
            uart_a.send('AUTH_FAIL')
            auth_state['status'] = 'fail'
            return
        # cosine similarity (q7 INT8)
        a = emb.astype(_np.float32)
        b = target.astype(_np.float32)
        sim = float(_np.dot(a, b) / (_np.linalg.norm(a) * _np.linalg.norm(b) + 1e-9))
        print(f'[SPK_AUTH] sim={sim:.3f} peak={peak} threshold={_SPEAKER_THRESHOLD}')
        if sim >= _SPEAKER_THRESHOLD:
            uart_a.send('AUTH_SUCCESS')
            auth_state['status'] = 'success'
        else:
            uart_a.send('AUTH_FAIL')
            auth_state['status'] = 'fail'
    except Exception as e:
        print(f'[SPK_AUTH] error: {e}')
        uart_a.send('AUTH_FAIL')
        auth_state['status'] = 'fail'


import random
import threading
import time
import base64
import glob
import os
import subprocess

app = Flask(__name__)

# 인증 상태 관리
auth_state = {
    'status':      'idle',
    'user_id':     None,
    'mission':     None,
    'stage':       None,
}

GESTURE_CLASSES = ['fist', 'open', 'index', 'victory', 'thumbsdown']
MAX_FAIL = 3

# ── UART 콜백 ─────────────────────────────────────────────────────────────────

def on_uart_message(msg):
    print(f"[UART] {msg}")

    if 'AUTH_SUCCESS' in msg:
        auth_state['status'] = 'success'
        if auth_state['user_id']:
            reset_fail(auth_state['user_id'])
            add_log(auth_state['user_id'], 'SUCCESS', auth_state['stage'])

    elif 'AUTH_FAIL' in msg:
        auth_state['status'] = 'fail'
        if auth_state['user_id']:
            fail_count = increment_fail(auth_state['user_id'])
            add_log(auth_state['user_id'], 'FAIL', auth_state['stage'])
            if fail_count >= MAX_FAIL:
                update_lock(auth_state['user_id'], True)
                auth_state['status'] = 'locked'
                uart.send('LOCK')

    elif 'AUTH_LOCKED' in msg:
        auth_state['status'] = 'locked'

    elif 'AUTH_UNLOCKED' in msg:
        auth_state['status'] = 'idle'

    elif msg.startswith('MISSION:'):
        mission_idx = int(msg.split(':')[1])
        auth_state['mission'] = GESTURE_CLASSES[mission_idx]
        auth_state['stage'] = 'gesture'

    elif 'Face detected' in msg:
        auth_state['stage'] = 'face'
    elif 'FaceID max_emb' in msg:
        auth_state['stage'] = 'faceid'

    elif 'SPEAKER_START' in msg:
        auth_state['stage'] = 'speaker'
        # 보드B에서 임베딩 받아서 cosine 비교 → AUTH_SUCCESS/FAIL
        _threading.Thread(target=_board_b_authenticate_async, daemon=True).start()

# ── API 엔드포인트 ────────────────────────────────────────────────────────────

@app.route('/auth/start', methods=['POST'])
def auth_start():
    data = request.get_json() or {}
    user_id = data.get('user_id', 1)

    user = get_user(user_id)
    if not user:
        return jsonify({'error': 'User not found'}), 404
    if user[3]:
        return jsonify({'error': 'User is locked'}), 403

    auth_state['status']  = 'authenticating'
    auth_state['user_id'] = user_id
    auth_state['mission'] = None
    auth_state['stage']   = 'face'

    uart.send(f'START:{user[2]}')

    return jsonify({
        'status':  'authenticating',
        'user_id': user_id
    })

@app.route('/auth/status', methods=['GET'])
def auth_status():
    return jsonify(auth_state)

@app.route('/enroll', methods=['POST'])
def enroll():
    data = request.get_json() or {}
    name  = data.get('name', 'Unknown')
    level = data.get('level', 1)

    user_id = add_user(name, level)

    return jsonify({
        'user_id': user_id,
        'name':    name,
        'level':   level
    })

@app.route('/enroll/capture', methods=['POST'])
def enroll_capture():
    uart.send('CAPTURE')
    return jsonify({'status': 'capture_requested'})

@app.route('/enroll/preview', methods=['GET'])
def enroll_preview():
    raws = sorted(glob.glob('/tmp/capture_*.raw'), reverse=True)
    if not raws:
        return jsonify({'error': 'no image'}), 404

    jpg_path = raws[0].replace('.raw', '.jpg')
    subprocess.run(['python', '/home/pi/max78000-auth/server/raw2jpg.py',
                    raws[0], jpg_path, '224', '168'])

    with open(jpg_path, 'rb') as f:
        b64 = base64.b64encode(f.read()).decode()
    return jsonify({'image': b64, 'path': raws[0]})

@app.route('/enroll/save', methods=['POST'])
def enroll_save():
    data = request.get_json() or {}
    raw_path = data.get('raw_path')
    user_name = data.get('user_name', 'unknown')
    index = data.get('index', 0)

    if not raw_path:
        return jsonify({'error': 'no raw_path'}), 400

    db_dir = f'/home/pi/max78000-auth/firmware/auth/db_gen/db_new/{user_name}'
    os.makedirs(db_dir, exist_ok=True)

    jpg_out = f'{db_dir}/face_{index:02d}.jpg'
    subprocess.run(['python', '/home/pi/max78000-auth/server/raw2jpg.py',
                    raw_path, jpg_out, '224', '168'])

    return jsonify({'saved': jpg_out})

@app.route('/enroll/finalize', methods=['POST'])
def enroll_finalize():
    data = request.get_json() or {}
    user_name = data.get('user_name', 'unknown')
    db_dir = f'/home/pi/max78000-auth/firmware/auth/db_gen/db_new/{user_name}'
    files = os.listdir(db_dir) if os.path.isdir(db_dir) else []
    
    if len(files) < 6:
        return jsonify({'error': f'need 6 images, got {len(files)}'}), 400
    
    # db_gen 실행
    db_gen_dir = '/home/pi/max78000-auth/firmware/auth/db_gen'
    env = os.environ.copy()
    env['PYTHONPATH'] = '/home/pi/ai8x-training:' + env.get('PYTHONPATH', '')
    
    proc = subprocess.run(
        ['/home/pi/ai8x-training/venv/bin/python', 
         'generate_face_db.py',
         '--db', 'db_new',
         '--emb', '../include/embeddings.h',
         '--weights', '../include/weights_3.h',
         '--base', '../include/baseaddr.h'],
        cwd=db_gen_dir,
        env=env,
        capture_output=True,
        text=True,
        timeout=300
    )
    
    return jsonify({
        'user': user_name,
        'images': len(files),
        'returncode': proc.returncode,
        'stdout': proc.stdout[-500:],
        'stderr': proc.stderr[-500:]
    })

@app.route('/enroll/speaker_capture', methods=['POST'])
def enroll_speaker_capture():
    """음성 캡처 시작 신호 (실제 녹음은 save에서 동기로 진행)"""
    return jsonify({'status': 'speaker_capture_ready'})

@app.route('/enroll/speaker_save', methods=['POST'])
def enroll_speaker_save():
    """보드B에 GO 보내고 EMBED 수신해서 _speaker_pending에 저장"""
    data = request.get_json() or {}
    user_name = data.get('user_name', 'unknown')
    index = data.get('index', 0)

    # 보드 B 시리얼 직접 오픈 (uart_b 백그라운드 쓰레드와 충돌 방지)
    ser = open_serial(_BOARD_B_PORT, 115200)
    if ser is None:
        return jsonify({'error': 'board B serial open failed'}), 500

    try:
        emb, peak, rms = recv_one(ser, timeout=30)
    finally:
        ser.close()

    if emb is None:
        return jsonify({'error': 'timeout / no embedding received',
                        'peak': peak, 'rms': rms}), 504

    _speaker_pending.setdefault(user_name, []).append(emb)
    return jsonify({
        'saved':   f'{user_name}_spk_{index:02d}',
        'peak':    int(peak),
        'rms':     int(rms),
        'count':   len(_speaker_pending[user_name]),
    })

@app.route('/enroll/speaker_finalize', methods=['POST'])
def enroll_speaker_finalize():
    """3개 임베딩 평균 내서 speaker_db.h 갱신"""
    data = request.get_json() or {}
    user_name = data.get('user_name', 'unknown')

    embs = _speaker_pending.get(user_name, [])
    if not embs:
        return jsonify({'error': f'no pending embeddings for {user_name}'}), 400

    avg_emb = average_embeddings(embs)

    # 기존 DB 로드 → 동일 이름 있으면 교체, 없으면 추가
    entries = load_header(_SPEAKER_DB_H)
    entries = [(n, e) for (n, e) in entries if n != user_name]
    entries.append((user_name, avg_emb))

    write_header(entries, _SPEAKER_DB_H)
    del _speaker_pending[user_name]

    return jsonify({
        'user':              user_name,
        'speaker_enrolled':  True,
        'utterances':        len(embs),
        'total_speakers':    len(entries),
        'db_path':           _SPEAKER_DB_H,
        'note':              'firmware rebuild & flash required',
    })

@app.route('/users', methods=['GET'])
def users():
    rows = get_all_users()
    result = []
    for r in rows:
        result.append({
            'id':         r[0],
            'name':       r[1],
            'level':      r[2],
            'locked':     bool(r[3]),
            'fail_count': r[4],
            'created_at': r[5]
        })
    return jsonify(result)

@app.route('/users/<int:user_id>', methods=['DELETE'])
def delete_user(user_id):
    import sqlite3
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('DELETE FROM users WHERE id = ?', (user_id,))
    c.execute('DELETE FROM auth_logs WHERE user_id = ?', (user_id,))
    conn.commit()
    conn.close()
    return jsonify({'deleted': user_id})

@app.route('/unlock', methods=['POST'])
def unlock():
    data = request.get_json() or {}
    user_id = data.get('user_id')
    if not user_id:
        return jsonify({'error': 'user_id required'}), 400

    update_lock(user_id, False)
    uart.send('UNLOCK')
    auth_state['status'] = 'idle'

    return jsonify({'status': 'unlocked', 'user_id': user_id})

@app.route('/logs', methods=['GET'])
def logs():
    limit = request.args.get('limit', 50, type=int)
    rows  = get_logs(limit)
    result = []
    for r in rows:
        result.append({
            'id':        r[0],
            'user_id':   r[1],
            'result':    r[2],
            'stage':     r[3],
            'timestamp': r[4]
        })
    return jsonify(result)

@app.route('/health', methods=['GET'])
def health():
    return jsonify({
        'status': 'ok',
        'uart':   uart.ser is not None and uart.ser.is_open
    })

# ── 메인 ──────────────────────────────────────────────────────────────────────

if __name__ == '__main__':
    init_db()
    # /tmp 기존 capture 파일 정리
    for f in glob.glob('/tmp/capture_*.raw') + glob.glob('/tmp/capture_*.jpg'):
        os.remove(f)
    uart.set_callback(on_uart_message)
    uart.connect()

    # 초기 dummy capture로 stale 데이터 정리
    def initial_dummy_capture():
        time.sleep(3)
        print("[INIT] Sending dummy CAPTURE to flush stale data...")
        uart.send('CAPTURE')
    threading.Thread(target=initial_dummy_capture, daemon=True).start()

    app.run(host='0.0.0.0', port=5000, debug=False)
