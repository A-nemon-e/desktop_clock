<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { getMe, logout } from '@/api'
import type { AuthUser } from '@/types'

const router = useRouter()
const route = useRoute()

const user = ref<AuthUser | null>(null)
const loading = ref(true)

const isLoginPage = () => route.path === '/login'

onMounted(async () => {
  if (isLoginPage()) {
    loading.value = false
    return
  }
  try {
    user.value = await getMe()
  } catch {
    user.value = null
  }
  loading.value = false
})

async function handleLogout() {
  try {
    await logout()
  } catch { /* ignore */ }
  user.value = null
  router.push('/login')
}
</script>

<template>
  <div v-if="loading" class="app-loading">
    <el-icon class="is-loading" :size="32"><Loading /></el-icon>
  </div>
  <template v-else-if="isLoginPage()">
    <router-view />
  </template>
  <template v-else>
    <el-container class="app-layout">
      <el-header class="app-header">
        <div class="header-left">
          <span class="app-title">Desktop Clock Admin</span>
          <el-menu
            mode="horizontal"
            :default-active="route.path"
            @select="(path: string) => router.push(path)"
            class="header-menu"
            :ellipsis="false"
          >
            <el-menu-item index="/devices">Devices</el-menu-item>
            <el-menu-item index="/firmware">Firmware</el-menu-item>
            <el-menu-item index="/users">Users</el-menu-item>
          </el-menu>
        </div>
        <div class="header-right" v-if="user">
          <span class="user-name">{{ user.username }}</span>
          <el-button text size="small" @click="handleLogout">Logout</el-button>
        </div>
      </el-header>
      <el-main class="app-main">
        <router-view />
      </el-main>
    </el-container>
  </template>
</template>

<style>
:root {
  --app-bg: #0a0a0f;
  --app-surface: #131320;
  --app-surface-hover: #1a1a2e;
  --app-border: #2a2a3c;
  --app-text: #e0e0f0;
  --app-text-dim: #8888a0;
  --app-accent: #6c5ce7;
  --app-accent-light: #a29bfe;
  --app-success: #00b894;
  --app-warning: #fdcb6e;
  --app-danger: #ff7675;
  --app-info: #74b9ff;
}

* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: 'SF Mono', 'Fira Code', 'Cascadia Code', 'JetBrains Mono', monospace;
  background: var(--app-bg);
  color: var(--app-text);
}

.app-loading {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 100vh;
  color: var(--app-accent-light);
}

.app-layout {
  min-height: 100vh;
}

.app-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  background: var(--app-surface);
  border-bottom: 1px solid var(--app-border);
  padding: 0 24px;
  height: 56px;
}

.header-left {
  display: flex;
  align-items: center;
  gap: 32px;
}

.app-title {
  font-size: 16px;
  font-weight: 700;
  color: var(--app-accent-light);
  letter-spacing: 0.5px;
}

.header-menu {
  border-bottom: none !important;
  background: transparent;
}

.header-menu .el-menu-item {
  color: var(--app-text-dim) !important;
  border-bottom: 2px solid transparent;
  font-size: 13px;
  height: 56px;
  line-height: 56px;
}

.header-menu .el-menu-item.is-active {
  color: var(--app-accent-light) !important;
  border-bottom-color: var(--app-accent) !important;
}

.header-menu .el-menu-item:hover {
  color: var(--app-text) !important;
  background: var(--app-surface-hover) !important;
}

.header-right {
  display: flex;
  align-items: center;
  gap: 12px;
}

.user-name {
  font-size: 13px;
  color: var(--app-text-dim);
}

.app-main {
  background: var(--app-bg);
  padding: 24px;
}

/* Override Element Plus dark theme */
.el-table {
  --el-table-bg-color: var(--app-surface);
  --el-table-tr-bg-color: var(--app-surface);
  --el-table-header-bg-color: var(--app-surface);
  --el-table-border-color: var(--app-border);
  --el-table-text-color: var(--app-text);
  --el-table-header-text-color: var(--app-text-dim);
}

.el-dialog {
  --el-dialog-bg-color: var(--app-surface);
  --el-dialog-border-color: var(--app-border);
}

.el-card {
  --el-card-bg-color: var(--app-surface);
  --el-card-border-color: var(--app-border);
}

.el-form-item__label {
  color: var(--app-text-dim) !important;
}

.el-input__wrapper,
.el-select__wrapper,
.el-input-number__wrapper {
  background-color: var(--app-bg) !important;
  box-shadow: 0 0 0 1px var(--app-border) inset !important;
}

.el-input__inner,
.el-select__placeholder {
  color: var(--app-text) !important;
}
</style>
