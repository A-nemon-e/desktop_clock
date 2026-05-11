<script setup lang="ts">
import { ref, reactive, computed, onMounted } from 'vue'
import {
  adminCreateFirmware,
  adminUploadFirmwareFile,
  adminListDevices,
  syncDeviceCards,
  type AdminDeviceItem,
} from '@/api'
import type { FirmwareData } from '@/types'
import { ElMessage } from 'element-plus'

const firmwares = ref<FirmwareData[]>([])
const devices = ref<AdminDeviceItem[]>([])
const loading = ref(false)
const uploading = ref(false)

const uploadForm = reactive({
  hw_rev: '',
  version: '',
  changelog: '',
})

const uploadFile = ref<File | null>(null)
const fileInputRef = ref<HTMLInputElement>()
const firmwareId = ref<string | null>(null)

const otaDialogVisible = ref(false)
const otaTargetMac = ref('')
const otaLoading = ref(false)

onMounted(() => {
  loadFirmware()
  loadDevices()
})

async function loadFirmware() {
  loading.value = true
  // TODO: add firmware list API endpoint
  loading.value = false
}

async function loadDevices() {
  try {
    const res = await adminListDevices()
    devices.value = res.devices
  } catch { /* ignore */ }
}

const firmwareByHwRev = computed(() => {
  const groups: Record<string, FirmwareData[]> = {}
  for (const fw of firmwares.value) {
    if (!groups[fw.hw_rev]) groups[fw.hw_rev] = []
    groups[fw.hw_rev].push(fw)
  }
  return groups
})

function onFileChange(e: Event) {
  const target = e.target as HTMLInputElement
  if (target.files?.length) {
    uploadFile.value = target.files[0]
  }
}

async function handleUpload() {
  if (!uploadForm.hw_rev || !uploadForm.version) {
    ElMessage.warning('HW Revision and Version are required')
    return
  }
  if (!uploadFile.value) {
    ElMessage.warning('Please select a firmware file')
    return
  }

  uploading.value = true
  try {
    const res = await adminCreateFirmware(uploadForm.hw_rev, uploadForm.version, uploadForm.changelog)
    await adminUploadFirmwareFile(res.id, uploadFile.value)
    ElMessage.success('Firmware uploaded successfully')
    uploadForm.hw_rev = ''
    uploadForm.version = ''
    uploadForm.changelog = ''
    uploadFile.value = null
    if (fileInputRef.value) fileInputRef.value.value = ''
    await loadFirmware()
  } catch (err: any) {
    ElMessage.error(err.response?.data?.detail || 'Upload failed')
  } finally {
    uploading.value = false
  }
}

const otableDevices = computed(() => {
  return devices.value.filter(d => !d.fw_version || d.hw_rev)
})

function showOtaDialog(mac: string) {
  otaTargetMac.value = mac
  otaDialogVisible.value = true
}

async function confirmOta() {
  otaLoading.value = true
  try {
    await syncDeviceCards(otaTargetMac.value, [], Date.now())
    ElMessage.success(`OTA triggered for ${otaTargetMac.value}`)
    otaDialogVisible.value = false
  } catch (err: any) {
    ElMessage.error(err.response?.data?.detail || 'OTA trigger failed')
  } finally {
    otaLoading.value = false
  }
}
</script>

<template>
  <div class="admin-firmware">
    <div class="page-header">
      <h2>Firmware Management</h2>
    </div>

    <el-row :gutter="24">
      <!-- Upload Form -->
      <el-col :span="12">
        <el-card>
          <template #header>
            <span class="card-title">Upload New Firmware</span>
          </template>
          <el-form label-position="top">
            <el-form-item label="Hardware Revision">
              <el-select v-model="uploadForm.hw_rev" placeholder="Select HW revision" style="width: 100%">
                <el-option label="0.1" value="0.1" />
                <el-option label="0.2" value="0.2" />
              </el-select>
            </el-form-item>
            <el-form-item label="Version">
              <el-input v-model="uploadForm.version" placeholder="e.g. 1.2.0" />
            </el-form-item>
            <el-form-item label="Changelog">
              <el-input
                v-model="uploadForm.changelog"
                type="textarea"
                :rows="3"
                placeholder="What changed in this version?"
              />
            </el-form-item>
            <el-form-item label="Firmware File (.bin)">
              <input
                ref="fileInputRef"
                type="file"
                accept=".bin"
                @change="onFileChange"
                style="color: var(--app-text)"
              />
            </el-form-item>
            <el-form-item>
              <el-button type="primary" :loading="uploading" @click="handleUpload">
                <el-icon><Upload /></el-icon>
                Upload Firmware
              </el-button>
            </el-form-item>
          </el-form>
        </el-card>
      </el-col>

      <!-- Firmware List -->
      <el-col :span="12">
        <el-card>
          <template #header>
            <span class="card-title">Firmware Versions</span>
          </template>
          <div v-if="Object.keys(firmwareByHwRev).length === 0" class="empty-state">
            No firmware uploaded yet
          </div>
          <div v-for="(fwList, hwRev) in firmwareByHwRev" :key="hwRev" class="fw-group">
            <h4 class="fw-hw-label">HW Rev {{ hwRev }}</h4>
            <div v-for="fw in fwList" :key="fw.id" class="fw-item">
              <div class="fw-version">{{ fw.version }}</div>
              <div class="fw-info">
                <span class="fw-date">{{ new Date(fw.created_at).toLocaleDateString() }}</span>
                <span v-if="fw.changelog" class="fw-changelog">{{ fw.changelog }}</span>
              </div>
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <!-- OTA Section -->
    <el-row :gutter="24" style="margin-top: 24px">
      <el-col :span="24">
        <el-card>
          <template #header>
            <span class="card-title">Trigger OTA Update</span>
          </template>
          <el-table :data="otableDevices" style="width: 100%" empty-text="No devices available for OTA">
            <el-table-column prop="mac" label="MAC" width="160" />
            <el-table-column prop="hw_rev" label="HW Rev" width="80" />
            <el-table-column prop="fw_version" label="Current FW" width="120">
              <template #default="{ row }">
                {{ row.fw_version || 'unknown' }}
              </template>
            </el-table-column>
            <el-table-column label="Action" width="120">
              <template #default="{ row }">
                <el-button size="small" type="warning" @click="showOtaDialog(row.mac)">
                  Trigger OTA
                </el-button>
              </template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-col>
    </el-row>

    <!-- OTA Confirm Dialog -->
    <el-dialog v-model="otaDialogVisible" title="Trigger OTA" width="400px">
      <p>Trigger OTA update for <strong>{{ otaTargetMac }}</strong>?</p>
      <template #footer>
        <el-button @click="otaDialogVisible = false">Cancel</el-button>
        <el-button type="warning" :loading="otaLoading" @click="confirmOta">
          Push Update
        </el-button>
      </template>
    </el-dialog>
  </div>
</template>

<style scoped>
.admin-firmware {
  max-width: 1400px;
  margin: 0 auto;
}

.page-header {
  margin-bottom: 20px;
}

.page-header h2 {
  font-size: 20px;
  font-weight: 600;
  color: var(--app-text);
}

.card-title {
  font-size: 14px;
  font-weight: 600;
  color: var(--app-text);
}

.empty-state {
  text-align: center;
  color: var(--app-text-dim);
  padding: 40px;
  font-size: 14px;
}

.fw-group {
  margin-bottom: 16px;
}

.fw-hw-label {
  font-size: 13px;
  color: var(--app-accent-light);
  margin-bottom: 8px;
  font-weight: 600;
}

.fw-item {
  display: flex;
  align-items: baseline;
  gap: 16px;
  padding: 8px 0;
  border-bottom: 1px solid var(--app-border);
}

.fw-item:last-child {
  border-bottom: none;
}

.fw-version {
  font-family: monospace;
  font-size: 14px;
  color: var(--app-text);
  font-weight: 600;
  min-width: 80px;
}

.fw-info {
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
}

.fw-date {
  font-size: 12px;
  color: var(--app-text-dim);
}

.fw-changelog {
  font-size: 12px;
  color: var(--app-text-dim);
  max-width: 300px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
</style>
