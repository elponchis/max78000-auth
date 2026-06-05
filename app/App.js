import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createNativeStackNavigator } from '@react-navigation/native-stack';

import HomeScreen   from './screens/HomeScreen';
import AuthScreen   from './screens/AuthScreen';
import EnrollScreen from './screens/EnrollScreen';
import AdminScreen  from './screens/AdminScreen';

const Stack = createNativeStackNavigator();

export default function App() {
  return (
    <NavigationContainer>
      <Stack.Navigator
        initialRouteName="Home"
        screenOptions={{
          headerStyle:      { backgroundColor: '#4A90E2' },
          headerTintColor:  '#fff',
          headerTitleStyle: { fontWeight: '700' },
        }}
      >
        <Stack.Screen
          name="Home"
          component={HomeScreen}
          options={{ title: 'Edge-Auth' }}
        />
        <Stack.Screen
          name="Auth"
          component={AuthScreen}
          options={{ title: '인증' }}
        />
        <Stack.Screen
          name="Enroll"
          component={EnrollScreen}
          options={{ title: '사용자 등록' }}
        />
        <Stack.Screen
          name="Admin"
          component={AdminScreen}
          options={{ title: '관리자' }}
        />
      </Stack.Navigator>
    </NavigationContainer>
  );
}
