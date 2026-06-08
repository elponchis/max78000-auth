from flask import Flask, jsonify, request
from db import init_db, add_user, get_user, get_all_users, \
               update_lock, increment_fail, reset_fail, add_log, get_logs
from uart import uart
import random
import threading
import time

app = Flask(__name__)

# 인증 상태 관리
auth_state = {
    'status':      'idle',   # idle / authenticating / success / fail / locked
    'user_id':     None,
    'mission':     None,     # 랜덤 제스처 미션 (0~4)
    'stage':       None,     # face / gesture / speaker
}

GESTURE_CLASSES = ['fist', 'open', 'index', 'victory', 'thumbsdown']
MAX_FAIL = 3

# ── UART 콜백 ─────────────────────────────────────────────────────────────────

def on_uart_message(msg):
    print(f"[UART] {msg}")

    if msg == 'AUTH_SUCCESS':
        auth_state['status'] = 'success'
        if auth_state['user_id']:
            reset_fail(auth_state['user_id'])
            add_log(auth_state['user_id'], 'SUCCESS', auth_state['stage'])

    elif msg == 'AUTH_FAIL':
        auth_state['status'] = 'fail'
        if auth_state['user_id']:
            fail_count = increment_fail(auth_state['user_id'])
            add_log(auth_state['user_id'], 'FAIL', auth_state['stage'])
            if fail_count >= MAX_FAIL:
                update_lock(auth_state['user_id'], True)
                auth_state['status'] = 'locked'
                uart.send('LOCK')

    elif msg == 'AUTH_LOCKED':
        auth_state['status'] = 'locked'

    elif msg == 'AUTH_UNLOCKED':
        auth_state['status'] = 'idle'

    elif msg.startswith('MISSION:'):
        # 보드에서 미션 요청 시 앱에 표시할 미션 업데이트
        mission_idx = int(msg.split(':')[1])
        auth_state['mission'] = GESTURE_CLASSES[mission_idx]
        auth_state['stage'] = 'gesture'

    elif 'Face detected' in msg:
        auth_state['stage'] = 'face'
        auth_state['status'] = 'face_detected'
    elif 'FaceID max_emb' in msg:
        # 얼굴 인식 진행 중
        auth_state['stage'] = 'faceid'
    elif 'Running gesture' in msg or 'Target: class' in msg:
        auth_state['stage'] = 'gesture'
    
    elif msg == 'SPEAKER_START':
        auth_state['stage'] = 'speaker'

# ── API 엔드포인트 ────────────────────────────────────────────────────────────

@app.route('/auth/start', methods=['POST'])
def auth_start():
    """인증 시작"""
    data = request.get_json() or {}
    user_id = data.get('user_id', 1)

    user = get_user(user_id)
    if not user:
        return jsonify({'error': 'User not found'}), 404
    if user[3]:  # locked
        return jsonify({'error': 'User is locked'}), 403

    # 랜덤 미션 생성
    mission_idx = random.randint(0, 4)
    mission = GESTURE_CLASSES[mission_idx]

    auth_state['status']  = 'authenticating'
    auth_state['user_id'] = user_id
    auth_state['mission'] = mission
    auth_state['stage']   = 'face'

    # 보드에 미션 전송
    uart.send(f'MISSION:{mission_idx}')

    return jsonify({
        'status':  'authenticating',
        'mission': mission,
        'user_id': user_id
    })

@app.route('/auth/status', methods=['GET'])
def auth_status():
    """현재 인증 상태 조회"""
    return jsonify(auth_state)

@app.route('/enroll', methods=['POST'])
def enroll():
    """사용자 등록"""
    data = request.get_json() or {}
    name  = data.get('name', 'Unknown')
    level = data.get('level', 1)

    user_id = add_user(name, level)
    uart.send(f'ENROLL:{user_id}')

    return jsonify({
        'user_id': user_id,
        'name':    name,
        'level':   level
    })

@app.route('/users', methods=['GET'])
def users():
    """전체 사용자 목록"""
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

@app.route('/unlock', methods=['POST'])
def unlock():
    """잠금 해제"""
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
    """인증 로그 조회"""
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
    """서버 상태 확인"""
    return jsonify({
        'status': 'ok',
        'uart':   uart.ser is not None and uart.ser.is_open
    })

# ── 메인 ──────────────────────────────────────────────────────────────────────

if __name__ == '__main__':
    init_db()
    uart.set_callback(on_uart_message)
    uart.connect()
    app.run(host='0.0.0.0', port=5000, debug=False)
