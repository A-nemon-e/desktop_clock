<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { adminListUsers, adminCreateUser, adminDeleteUser, bindDevice } from '@/api'
import type { UserData } from '@/types'
import { ElMessage, ElMessageBox } from 'element-plus'

const users = ref<UserData[]>([])
const total = ref(0)
const loading = ref(false)

const createDialogVisible = ref(false)
const createForm = ref({
  username: '',
  password: '',
})
const creating = ref(false)

const bindDialogVisible = ref(false)
const bindTargetUser = ref<UserData | null>(null)
const bindMac = ref('')
const binding = ref(false)

onMounted(() => {
  loadUsers()
})

async function loadUsers() {
  loading.value = true
  try {
    const res = await adminListUsers()
    users.value = res.users
    total.value = res.total
  } catch (err: any) {
    ElMessage.error(err.response?.data?.detail || 'Failed to load users')
  } finally {
    loading.value = false
  }
}

async function handleCreate() {
  if (!createForm.value.username || !createForm.value.password) {
    ElMessage.warning('Username and password are required')
    return
  }
  creating.value = true
  try {
    await adminCreateUser(createForm.value.username, createForm.value.password)
    ElMessage.success('User created')
    createDialogVisible.value = false
    createForm.value = { username: '', password: '' }
    await loadUsers()
  } catch (err: any) {
    ElMessage.error(err.response?.data?.detail || 'Create failed')
  } finally {
    creating.value = false
  }
}

async function handleDelete(user: UserData) {
  try {
    await ElMessageBox.confirm(
      `Delete user "${user.username}"?`,
      'Confirm Delete',
      { type: 'warning', confirmButtonText: 'Delete' }
    )
  } catch {
    return
  }

  try {
    await adminDeleteUser(user.id)
    ElMessage.success('User deleted')
    await loadUsers()
  } catch (err: any) {
    ElMessage.error(err.response?.data?.detail || 'Delete failed')
  }
}

function showBindDialog(user: UserData) {
  bindTargetUser.value = user
  bindMac.value = ''
  bindDialogVisible.value = true
}

async function handleBind() {
  if (!bindMac.value) {
    ElMessage.warning('MAC address is required')
    return
  }
  binding.value = true
  try {
    await bindDevice(bindMac.value)
    ElMessage.success('Device bound successfully')
    bindDialogVisible.value = false
  } catch (err: any) {
    ElMessage.error(err.response?.data?.detail || 'Bind failed')
  } finally {
    binding.value = false
  }
}
</script>

<template>
  <div class="admin-users">
    <div class="page-header">
      <h2>Users</h2>
      <div class="header-actions">
        <el-button type="primary" @click="createDialogVisible = true">
          <el-icon><Plus /></el-icon>
          Create User
        </el-button>
        <el-button @click="loadUsers" :loading="loading">
          <el-icon><Refresh /></el-icon>
        </el-button>
      </div>
    </div>

    <el-table
      :data="users"
      v-loading="loading"
      stripe
      style="width: 100%"
      empty-text="No users found"
    >
      <el-table-column prop="username" label="Username" min-width="160" />
      <el-table-column label="Admin" width="100" align="center">
        <template #default="{ row }">
          <el-tag :type="row.is_admin ? 'warning' : 'info'" size="small" effect="dark">
            {{ row.is_admin ? 'Admin' : 'User' }}
          </el-tag>
        </template>
      </el-table-column>
      <el-table-column label="Created" width="180">
        <template #default="{ row }">
          {{ new Date(row.created_at).toLocaleString() }}
        </template>
      </el-table-column>
      <el-table-column label="Actions" width="220" fixed="right">
        <template #default="{ row }">
          <el-button size="small" @click="showBindDialog(row)">Bind Device</el-button>
          <el-button size="small" type="danger" @click="handleDelete(row)" :disabled="row.is_admin">
            Delete
          </el-button>
        </template>
      </el-table-column>
    </el-table>

    <!-- Create User Dialog -->
    <el-dialog v-model="createDialogVisible" title="Create User" width="420px">
      <el-form label-position="top">
        <el-form-item label="Username">
          <el-input v-model="createForm.username" placeholder="Enter username" />
        </el-form-item>
        <el-form-item label="Password">
          <el-input v-model="createForm.password" type="password" placeholder="Enter password" show-password />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="createDialogVisible = false">Cancel</el-button>
        <el-button type="primary" :loading="creating" @click="handleCreate">Create</el-button>
      </template>
    </el-dialog>

    <!-- Bind Device Dialog -->
    <el-dialog v-model="bindDialogVisible" title="Bind Device" width="420px">
      <p class="bind-hint">
        Bind a device to user <strong>{{ bindTargetUser?.username }}</strong>
      </p>
      <el-form label-position="top" style="margin-top: 16px">
        <el-form-item label="Device MAC Address">
          <el-input v-model="bindMac" placeholder="aa:bb:cc:dd:ee:ff" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="bindDialogVisible = false">Cancel</el-button>
        <el-button type="primary" :loading="binding" @click="handleBind">Bind</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<style scoped>
.admin-users {
  max-width: 1200px;
  margin: 0 auto;
}

.page-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 20px;
}

.page-header h2 {
  font-size: 20px;
  font-weight: 600;
  color: var(--app-text);
}

.header-actions {
  display: flex;
  gap: 8px;
}

.bind-hint {
  font-size: 14px;
  color: var(--app-text-dim);
}
</style>
