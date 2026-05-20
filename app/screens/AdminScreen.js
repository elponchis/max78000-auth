import React, { useState, useEffect } from 'react';
import {
  View, Text, TouchableOpacity, StyleSheet,
  ScrollView, ActivityIndicator, Alert
} from 'react-native';
import { getUsers, unlockUser, getLogs } from '../services/api';

export default function AdminScreen({ navigation }) {
  const [users, setUsers]     = useState([]);
  const [logs, setLogs]       = useState([]);
  const [loading, setLoading] = useState(true);
  const [tab, setTab]         = useState('users'); // users / logs

  useEffect(() => {
    fetchData();
  }, []);

  const fetchData = async () => {
    try {
      setLoading(true);
      const [u, l] = await Promise.all([getUsers(), getLogs()]);
      setUsers(u.data);
      setLogs(l.data);
    } catch (e) {
      Alert.alert('오류', '데이터 로드 실패: ' + e.message);
    } finally {
      setLoading(false);
    }
  };

  const handleUnlock = async (user) => {
    Alert.alert(
      '잠금 해제',
      `${user.name}의 잠금을 해제하시겠습니까?`,
      [
        { text: '취소', style: 'cancel' },
        {
          text: '해제',
          onPress: async () => {
            try {
              await unlockUser(user.id);
              Alert.alert('완료', '잠금이 해제되었습니다.');
              fetchData();
            } catch (e) {
              Alert.alert('오류', e.message);
            }
          }
        }
      ]
    );
  };

  const getResultColor = (result) => {
    if (result === 'SUCCESS') return '#4CAF50';
    if (result === 'FAIL')    return '#F44336';
    return '#999';
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>관리자 화면</Text>

      {/* 탭 */}
      <View style={styles.tabRow}>
        <TouchableOpacity
          style={[styles.tab, tab === 'users' && styles.tabActive]}
          onPress={() => setTab('users')}
        >
          <Text style={[styles.tabText, tab === 'users' && styles.tabTextActive]}>
            사용자
          </Text>
        </TouchableOpacity>
        <TouchableOpacity
          style={[styles.tab, tab === 'logs' && styles.tabActive]}
          onPress={() => setTab('logs')}
        >
          <Text style={[styles.tabText, tab === 'logs' && styles.tabTextActive]}>
            인증 로그
          </Text>
        </TouchableOpacity>
      </View>

      {loading ? (
        <ActivityIndicator color="#4A90E2" style={{ marginTop: 32 }} />
      ) : (
        <ScrollView>
          {tab === 'users' && (
            <View>
              {users.map(user => (
                <View key={user.id} style={styles.card}>
                  <View style={styles.cardHeader}>
                    <View>
                      <Text style={styles.userName}>{user.name}</Text>
                      <Text style={styles.userSub}>
                        ID: {user.id} · 레벨 {user.level}
                      </Text>
                      <Text style={styles.userSub}>
                        실패 횟수: {user.fail_count}회
                      </Text>
                      <Text style={styles.userSub}>
                        등록일: {user.created_at?.slice(0, 10)}
                      </Text>
                    </View>
                    <View style={styles.badgeCol}>
                      <View style={[styles.badge,
                        { backgroundColor: user.locked ? '#FFEBEE' : '#E8F5E9' }]}>
                        <Text style={[styles.badgeText,
                          { color: user.locked ? '#F44336' : '#4CAF50' }]}>
                          {user.locked ? '🔒 잠금' : '✅ 정상'}
                        </Text>
                      </View>
                      {user.locked && (
                        <TouchableOpacity
                          style={styles.unlockBtn}
                          onPress={() => handleUnlock(user)}
                        >
                          <Text style={styles.unlockBtnText}>잠금 해제</Text>
                        </TouchableOpacity>
                      )}
                    </View>
                  </View>
                </View>
              ))}
            </View>
          )}

          {tab === 'logs' && (
            <View>
              {logs.length === 0 ? (
                <Text style={styles.emptyText}>인증 로그가 없습니다</Text>
              ) : (
                logs.map(log => (
                  <View key={log.id} style={styles.logItem}>
                    <View style={[styles.logDot,
                      { backgroundColor: getResultColor(log.result) }]} />
                    <View style={{ flex: 1 }}>
                      <Text style={styles.logText}>
                        사용자 {log.user_id} ·
                        <Text style={{ color: getResultColor(log.result) }}>
                          {' '}{log.result}
                        </Text>
                        {' '}· {log.stage}
                      </Text>
                      <Text style={styles.logTime}>
                        {log.timestamp?.slice(0, 19).replace('T', ' ')}
                      </Text>
                    </View>
                  </View>
                ))
              )}
            </View>
          )}
        </ScrollView>
      )}

      {/* 새로고침 */}
      <TouchableOpacity style={styles.refreshBtn} onPress={fetchData}>
        <Text style={styles.refreshBtnText}>🔄 새로고침</Text>
      </TouchableOpacity>

      <TouchableOpacity
        style={styles.backBtn}
        onPress={() => navigation.goBack()}
      >
        <Text style={styles.backBtnText}>← 뒤로</Text>
      </TouchableOpacity>
    </View>
  );
}

const styles = StyleSheet.create({
  container:       { flex: 1, backgroundColor: '#F5F7FA', padding: 16 },
  title:           { fontSize: 24, fontWeight: '700', color: '#333',
                     marginBottom: 16, marginTop: 8 },
  tabRow:          { flexDirection: 'row', marginBottom: 16,
                     backgroundColor: '#E8ECF0', borderRadius: 10, padding: 4 },
  tab:             { flex: 1, padding: 10, alignItems: 'center', borderRadius: 8 },
  tabActive:       { backgroundColor: '#fff', elevation: 2 },
  tabText:         { fontSize: 14, color: '#999', fontWeight: '600' },
  tabTextActive:   { color: '#4A90E2' },
  card:            { backgroundColor: '#fff', borderRadius: 12, padding: 16,
                     marginBottom: 10, elevation: 2 },
  cardHeader:      { flexDirection: 'row', justifyContent: 'space-between' },
  userName:        { fontSize: 16, fontWeight: '700', color: '#333' },
  userSub:         { fontSize: 12, color: '#999', marginTop: 2 },
  badgeCol:        { alignItems: 'flex-end', gap: 8 },
  badge:           { paddingHorizontal: 10, paddingVertical: 4, borderRadius: 20 },
  badgeText:       { fontSize: 12, fontWeight: '600' },
  unlockBtn:       { backgroundColor: '#4A90E2', paddingHorizontal: 12,
                     paddingVertical: 6, borderRadius: 8 },
  unlockBtnText:   { color: '#fff', fontSize: 12, fontWeight: '600' },
  logItem:         { backgroundColor: '#fff', borderRadius: 10, padding: 12,
                     marginBottom: 8, flexDirection: 'row',
                     alignItems: 'center', elevation: 1 },
  logDot:          { width: 10, height: 10, borderRadius: 5, marginRight: 12 },
  logText:         { fontSize: 14, color: '#333' },
  logTime:         { fontSize: 11, color: '#999', marginTop: 2 },
  emptyText:       { textAlign: 'center', color: '#999', marginTop: 32 },
  refreshBtn:      { backgroundColor: '#fff', borderRadius: 12, padding: 14,
                     alignItems: 'center', marginTop: 8, elevation: 2 },
  refreshBtnText:  { color: '#4A90E2', fontSize: 14, fontWeight: '600' },
  backBtn:         { padding: 12, alignItems: 'center' },
  backBtnText:     { color: '#999', fontSize: 14 },
});
