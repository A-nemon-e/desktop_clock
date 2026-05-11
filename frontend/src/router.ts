import { createRouter, createWebHistory } from 'vue-router'

const routes = [
  {
    path: '/login',
    name: 'Login',
    component: () => import('@/views/Login.vue'),
  },
  {
    path: '/',
    redirect: '/devices',
  },
  {
    path: '/devices',
    name: 'AdminDevices',
    component: () => import('@/views/AdminDevices.vue'),
  },
  {
    path: '/firmware',
    name: 'AdminFirmware',
    component: () => import('@/views/AdminFirmware.vue'),
  },
  {
    path: '/users',
    name: 'AdminUsers',
    component: () => import('@/views/AdminUsers.vue'),
  },
  {
    path: '/cards/:mac',
    name: 'CardEditor',
    component: () => import('@/views/CardEditor.vue'),
  },
]

const router = createRouter({
  history: createWebHistory(),
  routes,
})

export default router
