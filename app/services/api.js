import axios from 'axios';

const BASE_URL = 'http://192.168.137.63:5000'; // 라즈베리파이 IP로 변경 필요

const api = axios.create({
  baseURL: BASE_URL,
  timeout: 10000,
});

export const authStart = (userId) =>
  api.post('/auth/start', { user_id: userId });

export const authStatus = () =>
  api.get('/auth/status');

export const enrollUser = (name, level) =>
  api.post('/enroll', { name, level });

export const getUsers = () =>
  api.get('/users');

export const unlockUser = (userId) =>
  api.post('/unlock', { user_id: userId });

export const getLogs = () =>
  api.get('/logs');

export const getHealth = () =>
  api.get('/health');

export default api;

export const deleteUser = (userId) => api.delete(`/users/${userId}`);

export const enrollCapture = () => api.post('/enroll/capture');
export const enrollPreview = () => api.get('/enroll/preview');
export const enrollSave = (rawPath, userName, index) =>
  api.post('/enroll/save', { raw_path: rawPath, user_name: userName, index });
export const enrollFinalize = (userName) =>
  api.post('/enroll/finalize', { user_name: userName });
