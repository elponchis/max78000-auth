import React, { useState, useEffect } from 'react';
import {
  View, Text, TouchableOpacity, StyleSheet,
  ActivityIndicator, Alert
} from 'react-native';
import { authStart, authStatus } from '../services/api';

const GESTURE_EMOJI = {
  fist:       '✊',
  open:       '✋',
  index:      '☝️',
  victory:    '✌️',
  thumbsdown: '👎',
};

const GESTURE_KO = {
  fist:       '주먹',
  open:       '손바닥',
  index:      '검지',
  victory:    'V사인',
  thumbsdown: '엄지 아래',
};

export default function AuthScreen({ route, navigation }) {
  const { user } = route.params;
  const [status, setStatus]   = useState('idle');
  const [mission, setMission] = useState(null);
  const [stage, setStage]     = useState(null);
  const [loading, setLoading] = useState(false);
  const [polling, setPolling] = useState(false);

  useEffect(() => {
    return () => setPolling(false);
  }, []);

  const startAuth = async () => {
    try {
      setLoading(true);
      const res = await authStart(user.id);
      setMission(res.data.mission);
      setStatus('authenticating');
      setStage('face');
      setPolling(true);
      pollStatus();
    } catch (e) {
      Alert.alert('오류', '인증 시작 실패: ' + e.message);
    } finally {
      setLoading(false);
    }
  };

  const pollStatus = async () => {
    let count = 0;
    const maxCount = 60; // 최대 30초

    const poll = async () => {
      if (!polling || count >= maxCount) return;
      try {
        const res = await authStatus();
        const s = res.data;
        setStatus(s.status);
        setStage(s.stage);
        if (s.mission) setMission(s.mission);

        if (s.status === 'success') {
          setPolling(false);
          return;
        }
        if (s.status === 'fail' || s.status === 'locked') {
          setPolling(false);
          return;
        }
        count++;
        setTimeout(poll, 500);
      } catch (e) {
        count++;
        setTimeout(poll, 500);
      }
    };
    poll();
  };

  const reset = () => {
    setStatus('idle');
    setMission(null);
    setStage(null);
    setPolling(false);
  };

  const renderStageIndicator = () => (
    <View style={styles.stageRow}>
      {['face', 'gesture', 'speaker'].map((s, i) => (
        <View key={s} style={styles.stageItem}>
          <View style={[
            styles.stageDot,
            stage === s && styles.stageDotActive,
            (status === 'success') && styles.stageDotDone,
          ]} />
          <Text style={styles.stageLabel}>
            {s === 'face' ? '얼굴' : s === 'gesture' ? '제스처' : '음성'}
          </Text>
        </View>
      ))}
    </View>
  );

  return (
    <View style={styles.container}>
      {/* 사용자 정보 */}
      <View style={styles.userCard}>
        <Text style={styles.userName}>{user.name}</Text>
        <Text style={styles.userSub}>보안 레벨 {user.level}</Text>
      </View>

      {/* 인증 상태 */}
      {status === 'idle' && (
        <View style={styles.centerBox}>
          <Text style={styles.idleText}>아래 버튼을 눌러 인증을 시작하세요</Text>
        </View>
      )}

      {status === 'authenticating' && (
        <View style={styles.centerBox}>
          {renderStageIndicator()}

          {stage === 'face' && (
            <View style={styles.missionBox}>
              <Text style={styles.missionLabel}>얼굴을 카메라에 위치시켜 주세요</Text>
              <Text style={styles.missionEmoji}>👤</Text>
              <ActivityIndicator color="#4A90E2" style={{ marginTop: 16 }} />
            </View>
          )}

          {stage === 'gesture' && mission && (
            <View style={styles.missionBox}>
              <Text style={styles.missionLabel}>다음 제스처를 취해주세요</Text>
              <Text style={styles.missionEmoji}>{GESTURE_EMOJI[mission]}</Text>
              <Text style={styles.missionName}>{GESTURE_KO[mission]}</Text>
              <ActivityIndicator color="#4A90E2" style={{ marginTop: 16 }} />
            </View>
          )}

          {stage === 'speaker' && (
            <View style={styles.missionBox}>
              <Text style={styles.missionLabel}>마이크에 대고 말해주세요</Text>
              <Text style={styles.missionEmoji}>🎙️</Text>
              <ActivityIndicator color="#4A90E2" style={{ marginTop: 16 }} />
            </View>
          )}
        </View>
      )}

      {status === 'success' && (
        <View style={styles.centerBox}>
          <Text style={styles.successEmoji}>✅</Text>
          <Text style={styles.successText}>인증 성공!</Text>
        </View>
      )}

      {status === 'fail' && (
        <View style={styles.centerBox}>
          <Text style={styles.failEmoji}>❌</Text>
          <Text style={styles.failText}>인증 실패</Text>
          <Text style={styles.failSub}>다시 시도해주세요</Text>
        </View>
      )}

      {status === 'locked' && (
        <View style={styles.centerBox}>
          <Text style={styles.failEmoji}>🔒</Text>
          <Text style={styles.failText}>계정 잠금</Text>
          <Text style={styles.failSub}>관리자에게 문의하세요</Text>
        </View>
      )}

      {/* 버튼 */}
      {status === 'idle' && (
        <TouchableOpacity
          style={styles.primaryBtn}
          onPress={startAuth}
          disabled={loading}
        >
          {loading
            ? <ActivityIndicator color="#fff" />
            : <Text style={styles.primaryBtnText}>인증 시작</Text>
          }
        </TouchableOpacity>
      )}

      {(status === 'success' || status === 'fail' || status === 'locked') && (
        <TouchableOpacity style={styles.secondaryBtn} onPress={reset}>
          <Text style={styles.secondaryBtnText}>다시 시도</Text>
        </TouchableOpacity>
      )}

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
  userCard:        { backgroundColor: '#4A90E2', borderRadius: 12, padding: 20,
                     alignItems: 'center', marginBottom: 24 },
  userName:        { fontSize: 22, fontWeight: '700', color: '#fff' },
  userSub:         { fontSize: 14, color: 'rgba(255,255,255,0.8)', marginTop: 4 },
  centerBox:       { flex: 1, justifyContent: 'center', alignItems: 'center' },
  idleText:        { fontSize: 16, color: '#999', textAlign: 'center' },
  stageRow:        { flexDirection: 'row', marginBottom: 32 },
  stageItem:       { alignItems: 'center', marginHorizontal: 16 },
  stageDot:        { width: 14, height: 14, borderRadius: 7,
                     backgroundColor: '#DDD', marginBottom: 4 },
  stageDotActive:  { backgroundColor: '#4A90E2' },
  stageDotDone:    { backgroundColor: '#4CAF50' },
  stageLabel:      { fontSize: 12, color: '#999' },
  missionBox:      { alignItems: 'center' },
  missionLabel:    { fontSize: 16, color: '#555', marginBottom: 16 },
  missionEmoji:    { fontSize: 80 },
  missionName:     { fontSize: 24, fontWeight: '700', color: '#333', marginTop: 8 },
  successEmoji:    { fontSize: 80 },
  successText:     { fontSize: 28, fontWeight: '700', color: '#4CAF50', marginTop: 16 },
  failEmoji:       { fontSize: 80 },
  failText:        { fontSize: 28, fontWeight: '700', color: '#F44336', marginTop: 16 },
  failSub:         { fontSize: 16, color: '#999', marginTop: 8 },
  primaryBtn:      { backgroundColor: '#4A90E2', borderRadius: 12, padding: 16,
                     alignItems: 'center', marginBottom: 12 },
  primaryBtnText:  { color: '#fff', fontSize: 16, fontWeight: '700' },
  secondaryBtn:    { backgroundColor: '#fff', borderRadius: 12, padding: 16,
                     alignItems: 'center', marginBottom: 12, elevation: 2 },
  secondaryBtnText:{ color: '#4A90E2', fontSize: 16, fontWeight: '600' },
  backBtn:         { padding: 12, alignItems: 'center' },
  backBtnText:     { color: '#999', fontSize: 14 },
});
