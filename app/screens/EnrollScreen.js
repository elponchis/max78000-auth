import React, { useState } from 'react';
import {
  View, Text, TextInput, TouchableOpacity, StyleSheet,
  ActivityIndicator, Alert, ScrollView
} from 'react-native';
import { enrollUser } from '../services/api';

export default function EnrollScreen({ navigation }) {
  const [name, setName]     = useState('');
  const [level, setLevel]   = useState(1);
  const [loading, setLoading] = useState(false);

  const handleEnroll = async () => {
    if (!name.trim()) {
      Alert.alert('오류', '이름을 입력해주세요.');
      return;
    }
    try {
      setLoading(true);
      const res = await enrollUser(name.trim(), level);
      Alert.alert(
        '등록 완료',
        `${name} 등록 완료!\n사용자 ID: ${res.data.user_id}`,
        [{ text: '확인', onPress: () => navigation.goBack() }]
      );
    } catch (e) {
      Alert.alert('오류', '등록 실패: ' + e.message);
    } finally {
      setLoading(false);
    }
  };

  return (
    <ScrollView style={styles.container}>
      <Text style={styles.title}>새 사용자 등록</Text>

      {/* 이름 입력 */}
      <View style={styles.card}>
        <Text style={styles.label}>이름</Text>
        <TextInput
          style={styles.input}
          placeholder="이름을 입력하세요"
          value={name}
          onChangeText={setName}
        />
      </View>

      {/* 보안 레벨 선택 */}
      <View style={styles.card}>
        <Text style={styles.label}>보안 레벨</Text>
        <View style={styles.levelRow}>
          <TouchableOpacity
            style={[styles.levelBtn, level === 1 && styles.levelBtnActive]}
            onPress={() => setLevel(1)}
          >
            <Text style={[styles.levelBtnText, level === 1 && styles.levelBtnTextActive]}>
              레벨 1
            </Text>
            <Text style={styles.levelDesc}>얼굴 + 제스처</Text>
          </TouchableOpacity>

          <TouchableOpacity
            style={[styles.levelBtn, level === 2 && styles.levelBtnActive]}
            onPress={() => setLevel(2)}
          >
            <Text style={[styles.levelBtnText, level === 2 && styles.levelBtnTextActive]}>
              레벨 2
            </Text>
            <Text style={styles.levelDesc}>얼굴 + 제스처 + 음성</Text>
          </TouchableOpacity>
        </View>
      </View>

      {/* 안내 */}
      <View style={styles.infoCard}>
        <Text style={styles.infoTitle}>📋 등록 안내</Text>
        <Text style={styles.infoText}>
          • 등록 후 보드 앞에서 얼굴 등록이 진행됩니다{'\n'}
          • 레벨 2는 추가로 목소리 등록이 필요합니다{'\n'}
          • 등록된 데이터는 보드 Flash에 안전하게 저장됩니다
        </Text>
      </View>

      {/* 등록 버튼 */}
      <TouchableOpacity
        style={styles.primaryBtn}
        onPress={handleEnroll}
        disabled={loading}
      >
        {loading
          ? <ActivityIndicator color="#fff" />
          : <Text style={styles.primaryBtnText}>등록하기</Text>
        }
      </TouchableOpacity>

      <TouchableOpacity
        style={styles.backBtn}
        onPress={() => navigation.goBack()}
      >
        <Text style={styles.backBtnText}>← 뒤로</Text>
      </TouchableOpacity>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container:          { flex: 1, backgroundColor: '#F5F7FA', padding: 16 },
  title:              { fontSize: 24, fontWeight: '700', color: '#333',
                        marginBottom: 24, marginTop: 8 },
  card:               { backgroundColor: '#fff', borderRadius: 12, padding: 16,
                        marginBottom: 12, elevation: 2 },
  label:              { fontSize: 14, fontWeight: '600', color: '#555',
                        marginBottom: 8 },
  input:              { borderWidth: 1, borderColor: '#E0E0E0', borderRadius: 8,
                        padding: 12, fontSize: 16, color: '#333' },
  levelRow:           { flexDirection: 'row', gap: 12 },
  levelBtn:           { flex: 1, borderWidth: 2, borderColor: '#E0E0E0',
                        borderRadius: 10, padding: 16, alignItems: 'center' },
  levelBtnActive:     { borderColor: '#4A90E2', backgroundColor: '#EBF3FD' },
  levelBtnText:       { fontSize: 16, fontWeight: '700', color: '#999' },
  levelBtnTextActive: { color: '#4A90E2' },
  levelDesc:          { fontSize: 12, color: '#999', marginTop: 4, textAlign: 'center' },
  infoCard:           { backgroundColor: '#FFF9E6', borderRadius: 12, padding: 16,
                        marginBottom: 24, borderLeftWidth: 4, borderLeftColor: '#FFC107' },
  infoTitle:          { fontSize: 14, fontWeight: '700', color: '#333', marginBottom: 8 },
  infoText:           { fontSize: 13, color: '#666', lineHeight: 22 },
  primaryBtn:         { backgroundColor: '#4A90E2', borderRadius: 12, padding: 16,
                        alignItems: 'center', marginBottom: 12 },
  primaryBtnText:     { color: '#fff', fontSize: 16, fontWeight: '700' },
  backBtn:            { padding: 12, alignItems: 'center' },
  backBtnText:        { color: '#999', fontSize: 14 },
});
