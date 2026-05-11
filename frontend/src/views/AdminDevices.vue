<script setup lang="ts">
import { ref, reactive, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { adminListDevices, getDevice, syncDeviceCards, type AdminDeviceItem } from '@/api'
import type { DeviceData } from '@/types'
import { ElMessage, ElMessageBox } from 'element-plus'

const router = useRouter()
const devices = ref<AdminDeviceItem[]>([])
const total = ref(0)
const loading = ref(false)

const filters = reactive({
  hwRev: '',
  onlineOnly: false,
})

const detailVisible = ref(false)
const detailDevice = ref<DeviceData | null>(null)
const detailLoading = ref(false)

onMounted(() => {
  loadDevices()
})

async function loadDevices() {
  loading.value = true
  try {
    const res = await adminListDevices()
    devices.value = res.devices
    total.value = res.total
  } catch (err: any) {
    ElMessage.error(err.response?.data?.detail || 'Failed to load devices')
  } finally {
    loading.value = false
  }
}

const filteredDevices = () => {
  let list = devices.value
  if (filters.hwRev) {
    list = list.filter(d => d.hw_rev === filters.hwRev)
  }
  if (filters.onlineOnly) {
    const now = Date.now()
    list = list.filter(d => {
      if (!d.last_seen) return false
      return now - new Date(d.last_seen).getTime() < 120_000
    })
  }
  return list
}

function isOnline(lastSeen: string | null): boolean {
  if (!lastSeen) return false
  return Date.now() - new Date(lastSeen).getTime() < 120_000
}

async function showDetail(mac: string) {
  detailVisible.value = true
  detailLoading.value = true
  try {
    detailDevice.value = await getDevice(mac)
  } catch {
    ElMessage.error('Failed to load device details')
  } finally {
    detailLoading.value = false
  }
}

function editCards(mac: string) {
  router.push(`/cards/${mac}`)
}

function triggerOTA(mac: string) {
  ElMessageBox.confirm(
    `Trigger OTA update for device ${mac}?`,
    'Confirm OTA',
    { confirmButtonText: 'Trigger', cancelButtonText: 'Cancel', type: 'warning' }
  ).then(async () => {
    try {
      await syncDeviceCards(mac, [], Date.now())
      ElMessage.success('OTA triggered')
    } catch (err: any) {
      ElMessage.error(err.response?.data?.detail || 'OTA trigger failed')
    }
  }).catch(() => {})
}
</script>

<template>
  <div class="admin-devices">
    <div class="page-header">
      <h2>Devices</h2>
      <el-button type="primary" @click="loadDevices" :loading="loading">
        <el-icon><Refresh /></el-icon>
        Refresh
      </el-button>
    </div>

    <div class="filters-bar">
      <el-select v-model="filters.hwRev" placeholder="HW Revision" clearable style="width: 160px">
        <el-option label="0.1" value="0.1" />
        <el-option label="0.2" value="0.2" />
      </el-select>
      <el-checkbox v-model="filters.onlineOnly">Online only</el-checkbox>
      <span class="filter-count">{{ filteredDevices().length }} / {{ total }} devices</span>
    </div>

    <el-table
      :data="filteredDevices()"
      v-loading="loading"
      stripe
      style="width: 100%"
      empty-text="No devices found"
    >
      <el-table-column prop="mac" label="MAC" width="160" />
      <el-table-column prop="hw_rev" label="HW Rev" width="80" />
      <el-table-column prop="fw_version" label="FW Version" width="100">
        <template #default="{ row }">
          {{ row.fw_version || '-' }}
        </template>
      </el-table-column>
      <el-table-column prop="ip" label="IP" width="140">
        <template #default="{ row }">
          {{ row.ip || '-' }}
        </template>
      </el-table-column>
      <el-table-column prop="region" label="Region" min-width="120">
        <template #default="{ row }">
          {{ row.region || '-' }}
        </template>
      </el-table-column>
      <el-table-column label="Online" width="80" align="center">
        <template #default="{ row }">
          <el-tag :type="isOnline(row.last_seen) ? 'success' : 'info'" size="small" effect="dark">
            {{ isOnline(row.last_seen) ? 'Online' : 'Offline' }}
          </el-tag>
        </template>
      </el-table-column>
      <el-table-column label="Last Seen" width="170">
        <template #default="{ row }">
          {{ row.last_seen ? new Date(row.last_seen).toLocaleString() : '-' }}
        </template>
      </el-table-column>
      <el-table-column label="Actions" width="280" fixed="right">
        <template #default="{ row }">
          <el-button size="small" @click="showDetail(row.mac)">Detail</el-button>
          <el-button size="small" @click="editCards(row.mac)">Cards</el-button>
          <el-button size="small" type="warning" @click="triggerOTA(row.mac)">OTA</el-button>
        </template>
      </el-table-column>
    </el-table>

    <!-- Device Detail Dialog -->
    <el-dialog v-model="detailVisible" title="Device Detail" width="520px">
      <div v-if="detailLoading" class="detail-loading">
        <el-icon class="is-loading" :size="24"><Loading /></el-icon>
      </div>
      <div v-else-if="detailDevice" class="detail-body">
        <div class="detail-row">
          <span class="detail-label">MAC</span>
          <span class="detail-value">{{ detailDevice.mac }}</span>
        </div>
        <div class="detail-row">
          <span class="detail-label">Hardware Revision</span>
          <span class="detail-value">{{ detailDevice.hw_rev }}</span>
        </div>
        <div class="detail-row">
          <span class="detail-label">Firmware Version</span>
          <span class="detail-value">{{ detailDevice.fw_version || '-' }}</span>
        </div>
        <div class="detail-row">
          <span class="detail-label">IP Address</span>
          <span class="detail-value">{{ detailDevice.ip || '-' }}</span>
        </div>
        <div class="detail-row">
          <span class="detail-label">Region</span>
          <span class="detail-value">{{ detailDevice.region || '-' }}</span>
        </div>
        <div class="detail-row">
          <span class="detail-label">Timezone</span>
          <span class="detail-value">{{ detailDevice.timezone || '-' }}</span>
        </div>
        <div class="detail-row">
          <span class="detail-label">Thin Mode</span>
          <span class="detail-value">{{ detailDevice.thin_mode }}</span>
        </div>
        <div class="detail-row" v-if="detailDevice.latitude != null">
          <span class="detail-label">Coordinates</span>
          <span class="detail-value">{{ detailDevice.latitude }}, {{ detailDevice.longitude }}</span>
        </div>
        <div class="detail-row">
          <span class="detail-label">Last Seen</span>
          <span class="detail-value">{{ new Date(detailDevice.last_seen).toLocaleString() }}</span>
        </div>
      </div>
    </el-dialog>
  </div>
</template>

<style scoped>
.admin-devices {
  max-width: 1400px;
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

.filters-bar {
  display: flex;
  align-items: center;
  gap: 16px;
  margin-bottom: 16px;
  flex-wrap: wrap;
}

.filter-count {
  font-size: 12px;
  color: var(--app-text-dim);
  margin-left: auto;
}

.detail-loading {
  display: flex;
  justify-content: center;
  padding: 40px;
}

.detail-body {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.detail-row {
  display: flex;
  align-items: baseline;
  gap: 12px;
}

.detail-label {
  font-size: 12px;
  color: var(--app-text-dim);
  min-width: 140px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.detail-value {
  font-size: 14px;
  color: var(--app-text);
  font-family: monospace;
}
</style>
