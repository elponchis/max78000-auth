import sqlite3
import os
from datetime import datetime

DB_PATH = os.path.join(os.path.dirname(__file__), 'edgeauth.db')

def init_db():
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()

    # 사용자 테이블
    c.execute('''
        CREATE TABLE IF NOT EXISTS users (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            name      TEXT NOT NULL,
            level     INTEGER DEFAULT 1,
            locked    INTEGER DEFAULT 0,
            fail_count INTEGER DEFAULT 0,
            created_at TEXT
        )
    ''')

    # 인증 로그 테이블
    c.execute('''
        CREATE TABLE IF NOT EXISTS auth_logs (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER,
            result     TEXT,
            stage      TEXT,
            timestamp  TEXT
        )
    ''')

    conn.commit()
    conn.close()
    print("DB initialized.")

def add_user(name, level=1):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute(
        'INSERT INTO users (name, level, created_at) VALUES (?, ?, ?)',
        (name, level, datetime.now().isoformat())
    )
    conn.commit()
    user_id = c.lastrowid
    conn.close()
    return user_id

def get_user(user_id):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('SELECT * FROM users WHERE id = ?', (user_id,))
    row = c.fetchone()
    conn.close()
    return row

def get_all_users():
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('SELECT * FROM users')
    rows = c.fetchall()
    conn.close()
    return rows

def update_lock(user_id, locked):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('UPDATE users SET locked = ?, fail_count = 0 WHERE id = ?',
              (1 if locked else 0, user_id))
    conn.commit()
    conn.close()

def increment_fail(user_id):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('UPDATE users SET fail_count = fail_count + 1 WHERE id = ?',
              (user_id,))
    conn.commit()
    c.execute('SELECT fail_count FROM users WHERE id = ?', (user_id,))
    fail_count = c.fetchone()[0]
    conn.close()
    return fail_count

def reset_fail(user_id):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('UPDATE users SET fail_count = 0 WHERE id = ?', (user_id,))
    conn.commit()
    conn.close()

def add_log(user_id, result, stage):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute(
        'INSERT INTO auth_logs (user_id, result, stage, timestamp) VALUES (?, ?, ?, ?)',
        (user_id, result, stage, datetime.now().isoformat())
    )
    conn.commit()
    conn.close()

def get_logs(limit=50):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('SELECT * FROM auth_logs ORDER BY id DESC LIMIT ?', (limit,))
    rows = c.fetchall()
    conn.close()
    return rows

if __name__ == '__main__':
    init_db()
