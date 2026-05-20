import React, { useState, useEffect } from 'react';
import {
  View, Text, TouchableOpacity, StyleSheet,
  ActivityIndicator, Alert, ScrollView
} from 'react-native';
import { getHealth, getUsers } from '../services/api';

export default function HomeScreen({ navigation }) {
  const [health, setHealth]   = useState(null);
  const [users, setUsers]     = useState([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 5000);
    return () => clearInterval(interval);
  }, []);

  const fetchData = async () => {
    try {
      const [h, u] = await Promise.all([getHealth(), getUsers()]);
      setHealth(h.data);
      setUsers(u.data);
    } catch (e) {
      console.log('서버 연결 실패:', e.message);
    } finally {
      setLoading(false);
    }
  };

  return (
    <ScrollView style={styles.container}>
      {/* 서버 상태 */}
      <View style={styles.statusCard}>
        <Text style={styles.cardTitle}>서버 상태</Text>
        {loading ? (
          <ActivityIndicator color="#4A90E2" />
        ) : (
          <View style={styles.row}>
            <View style={[styles.dot,
              { backgroundColor: health ? '#4CAF50' : '#F44336' }]} />
            <Text style={styles.statusText}>
              {health ? '연결됨' : '연결 안 됨'}
            </Text>
            <View style={[styles.dot,
              { backgroundColor: health?.uart ? '#4CAF50' : '#FF9800', marginLeft: 16 }]} />
            <Text style={styles.statusText}>
              UART {health?.uart ? '연결됨' : '연결 안 됨'}
            </Text>
          </View>
        )}
      </View>

      {/* 사용자 목록 */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>등록된 사용자</Text>
        {users.length === 0 ? (
          <Text style={styles.emptyText}>등록된 사용자가 없습니다</Text>
        ) : (
          users.map(user => (
            <TouchableOpacity
              key={user.id}
              style={[styles.userItem, user.locked && styles.lockedItem]}
              onPress={() => navigation.navigate('Auth', { user })}
            >
              <View>
                <Text style={styles.userName}>{user.name}</Text>
                <Text style={styles.userSub}>
                  레벨 {user.level} · 실패 {user.fail_count}회
                </Text>
              </View>
              <View style={styles.row}>
                {user.locked && (
                  <Text style={styles.lockedBadge}>🔒 잠금</Text>
                )}
                <Text style={styles.arrow}>›</Text>
              </View>
            </TouchableOpacity>
          ))
        )}
      </View>

      {/* 버튼 */}
      <TouchableOpacity
        style={styles.primaryBtn}
        onPress={() => navigation.navigate('Enroll')}
      >
        <Text style={styles.primaryBtnText}>+ 새 사용자 등록</Text>
      </TouchableOpacity>

      <TouchableOpacity
        style={styles.secondaryBtn}
        onPress={() => navigation.navigate('Admin')}
      >
        <Text style={styles.secondaryBtnText}>관리자 화면</Text>
      </TouchableOpacity>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container:      { flex: 1, backgroundColor: '#F5F7FA', padding: 16 },
  statusCard:     { backgroundColor: '#fff', borderRadius: 12, padding: 16,
                    marginBottom: 12, elevation: 2 },
  card:           { backgroundColor: '#fff', borderRadius: 12, padding: 16,
                    marginBottom: 12, elevation: 2 },
  cardTitle:      { fontSize: 16, fontWeight: '700', color: '#333', marginBottom: 12 },
  row:            { flexDirection: 'row', alignItems: 'center' },
  dot:            { width: 10, height: 10, borderRadius: 5, marginRight: 6 },
  statusText:     { fontSize: 14, color: '#555' },
  emptyText:      { fontSize: 14, color: '#999', textAlign: 'center', padding: 16 },
  userItem:       { flexDirection: 'row', justifyContent: 'space-between',
                    alignItems: 'center', paddingVertical: 12,
                    borderBottomWidth: 1, borderBottomColor: '#F0F0F0' },
  lockedItem:     { opacity: 0.6 },
  userName:       { fontSize: 16, fontWeight: '600', color: '#333' },
  userSub:        { fontSize: 12, color: '#999', marginTop: 2 },
  lockedBadge:    { fontSize: 12, color: '#F44336', marginRight: 8 },
  arrow:          { fontSize: 20, color: '#CCC' },
  primaryBtn:     { backgroundColor: '#4A90E2', borderRadius: 12, padding: 16,
                    alignItems: 'center', marginBottom: 12 },
  primaryBtnText: { color: '#fff', fontSize: 16, fontWeight: '700' },
  secondaryBtn:   { backgroundColor: '#fff', borderRadius: 12, padding: 16,
                    alignItems: 'center', marginBottom: 24, elevation: 2 },
  secondaryBtnText: { color: '#4A90E2', fontSize: 16, fontWeight: '600' },
});
