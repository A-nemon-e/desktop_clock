<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { getDevice, getDeviceCards, syncDeviceCards, adminListDevices, type CardSyncItem, type AdminDeviceItem } from '@/api'
import type { DeviceData, CardConfig, FgSlot, BgType, FgType } from '@/types'
import {
  BG_TYPES, BG_LABELS, BG_NO_FOREGROUND, BG_SIMPLIFIED,
  FG_TYPES, FG_LABELS, FONT_SIZES,
  MAX_CARDS, MAX_FG_PER_CARD,
} from '@/types'
import { ElMessage } from 'element-plus'

const route = useRoute()
const router = useRouter()
const mac = ref((route.params.mac as string) || '')

const device = ref<DeviceData | null>(null)
const allDevices = ref<AdminDeviceItem[]>([])
const cards = ref<(CardConfig & { position: number })[]>([])
const selectedIndex = ref(-1)
const saving = ref(false)
const loading = ref(true)
const error = ref<string | null>(null)

onMounted(async () => {
  await loadDevices()
  if (mac.value) {
    await loadDeviceData()
  }
})

async function loadDevices() {
  try {
    const res = await adminListDevices()
    allDevices.value = res.devices
  } catch { /* non-critical */ }
}

async function loadDeviceData() {
  loading.value = true
  error.value = null
  try {
    device.value = await getDevice(mac.value)
    const res = await getDeviceCards(mac.value)
    const loaded = res.cards || []

    const existing = new Array(MAX_CARDS).fill(null).map((_, i) => {
      const card = loaded.find(c => c.position === i)
      if (card) {
        const cfg = typeof card.fg_config === 'object' && card.fg_config !== null && !Array.isArray(card.fg_config)
          ? (card.fg_config as unknown as CardConfig)
          : { bg: 'fire' as BgType, fgs: [], fg_count: 0, relative_brightness: 0, switch_interval: 30 }

        return {
          ...cfg,
          position: i,
          relative_brightness: card.rel_brightness != null ? mapRelBrightnessFromApi(card.rel_brightness) : cfg.relative_brightness,
          switch_interval: card.duration || cfg.switch_interval,
        }
      }
      return createEmptyCard(i)
    })

    cards.value = existing
  } catch (err: any) {
    error.value = err.response?.data?.detail || 'Failed to load device'
  } finally {
    loading.value = false
  }
}

function onDeviceChange(newMac: string) {
  if (newMac === mac.value) return
  mac.value = newMac
  router.replace(`/cards/${newMac}`)
  selectedIndex.value = -1
  loadDeviceData()
}

function createEmptyCard(pos: number): CardConfig & { position: number } {
  return {
    bg: 'fire' as BgType,
    fgs: [createDefaultFg('hms')],
    fg_count: 1,
    relative_brightness: 0,
    switch_interval: 30,
    position: pos,
  }
}

function createDefaultFg(type: FgType): FgSlot {
  return {
    type,
    font: type === 'temp_humid' ? '5x5' : 'long',
    duration_ms: 5000,
  }
}

function mapRelBrightnessFromApi(apiVal: number): number {
  return Math.round((apiVal - 100) * 128 / 100)
}

function mapRelBrightnessToApi(internalVal: number): number {
  return Math.round(100 + (internalVal * 100) / 127)
}

const selectedCard = computed(() => {
  if (selectedIndex.value < 0 || selectedIndex.value >= cards.value.length) return null
  return cards.value[selectedIndex.value]
})

const selectedBg = computed<BgType>(() => selectedCard.value?.bg || 'fire')

const isNoFg = computed(() => BG_NO_FOREGROUND.includes(selectedBg.value))
const isSimplified = computed(() => BG_SIMPLIFIED.includes(selectedBg.value))

const bgOptions = computed(() => {
  return Object.entries(BG_LABELS).map(([key, label]) => {
    const bg = key as BgType
    let hint = ''
    if (BG_NO_FOREGROUND.includes(bg)) hint = '(no foregrounds)'
    else if (BG_SIMPLIFIED.includes(bg)) hint = '(simplified toggles)'
    else if (bg === 'fire') hint = '(font locked to 3×5)'
    return { value: bg, label: `${label} ${hint}`.trim() }
  })
})

function selectCard(index: number) {
  selectedIndex.value = index
}

function setBg(bg: BgType) {
  if (!selectedCard.value) return
  selectedCard.value.bg = bg

  if (BG_NO_FOREGROUND.includes(bg)) {
    selectedCard.value.fgs = []
    selectedCard.value.fg_count = 0
  } else if (selectedCard.value.fg_count === 0) {
    selectedCard.value.fgs = [createDefaultFg('hms')]
    selectedCard.value.fg_count = 1
  }

  if (bg === 'fire') {
    for (const fg of selectedCard.value.fgs) {
      fg.font = '3x5'
    }
  }
}

function addForeground() {
  if (!selectedCard.value) return
  if (isNoFg.value) return
  if (selectedCard.value.fg_count >= MAX_FG_PER_CARD) return

  const newFg = createDefaultFg('hms')
  if (selectedCard.value.bg === 'fire') {
    newFg.font = '3x5'
  }
  selectedCard.value.fgs.push(newFg)
  selectedCard.value.fg_count = selectedCard.value.fgs.length
}

function removeForeground(index: number) {
  if (!selectedCard.value) return
  selectedCard.value.fgs.splice(index, 1)
  selectedCard.value.fg_count = selectedCard.value.fgs.length
}

function moveFg(from: number, to: number) {
  if (!selectedCard.value) return
  const fgs = selectedCard.value.fgs
  const item = fgs.splice(from, 1)[0]
  fgs.splice(to, 0, item)
}

function setFgType(index: number, type: FgType) {
  if (!selectedCard.value) return
  selectedCard.value.fgs[index].type = type
  if (type === 'temp_humid') {
    selectedCard.value.fgs[index].font = '5x5'
  }
}

// ── Drag-sort for card positions ────────────────────────

let dragIndex = -1

function onDragStart(index: number, e: DragEvent) {
  dragIndex = index
  if (e.dataTransfer) {
    e.dataTransfer.effectAllowed = 'move'
    e.dataTransfer.setData('text/plain', String(index))
  }
}

function onDragOver(index: number, e: DragEvent) {
  e.preventDefault()
  if (e.dataTransfer) e.dataTransfer.dropEffect = 'move'
}

function onDrop(index: number) {
  if (dragIndex < 0 || dragIndex === index) return
  const item = cards.value.splice(dragIndex, 1)[0]
  cards.value.splice(index, 0, item)
  cards.value.forEach((c, i) => { c.position = i })
  dragIndex = -1
}

// ── Save ────────────────────────────────────────────────

async function saveCards() {
  saving.value = true
  try {
    const payload: CardSyncItem[] = cards.value.map(c => ({
      bg_type: c.bg,
      fg_config: {
        bg: c.bg,
        fgs: c.fgs,
        fg_count: c.fg_count,
        relative_brightness: c.relative_brightness,
        switch_interval: c.switch_interval,
      },
      duration: c.switch_interval,
      rel_brightness: mapRelBrightnessToApi(c.relative_brightness),
      position: c.position,
    }))

    await syncDeviceCards(mac.value, payload, Date.now())
    ElMessage.success('Cards saved and pushed to device')
  } catch (err: any) {
    ElMessage.error(err.response?.data?.detail || 'Save failed')
  } finally {
    saving.value = false
  }
}
</script>

<template>
  <div class="card-editor" v-loading="loading">
    <div v-if="error" class="error-state">{{ error }}</div>

    <template v-else-if="device">
      <div class="editor-header">
        <div class="header-left-col">
          <h2>Card Editor</h2>
          <el-select
            :model-value="mac"
            @update:model-value="onDeviceChange"
            placeholder="Select device"
            filterable
            style="width: 280px"
          >
            <el-option
              v-for="d in allDevices"
              :key="d.mac"
              :label="`${d.mac} (${d.hw_rev})`"
              :value="d.mac"
            />
          </el-select>
        </div>
        <span class="device-meta">{{ device.hw_rev }} / FW {{ device.fw_version || '-' }}</span>
      </div>

      <div class="editor-body">
        <!-- Left: Card Slots -->
        <div class="card-slots-panel">
          <h3 class="panel-title">Card Slots ({{ cards.length }} max)</h3>
          <div class="slots-grid">
            <div
              v-for="(card, index) in cards"
              :key="index"
              class="slot"
              :class="{
                'slot-selected': selectedIndex === index,
                'slot-occupied': card.fg_count > 0 || card.bg !== 'fire',
              }"
              draggable="true"
              @dragstart="onDragStart(index, $event)"
              @dragover="onDragOver(index, $event)"
              @drop="onDrop(index)"
              @click="selectCard(index)"
            >
              <span class="slot-number">{{ index + 1 }}</span>
              <span class="slot-bg">{{ BG_LABELS[card.bg] }}</span>
              <span class="slot-detail" v-if="card.fg_count > 0">
                {{ card.fg_count }} fg · {{ card.switch_interval }}s
              </span>
              <span class="slot-detail" v-else>empty</span>
            </div>
          </div>
        </div>

        <!-- Right: Properties Panel -->
        <div class="props-panel" v-if="selectedCard">
          <h3 class="panel-title">Slot {{ selectedIndex + 1 }} Properties</h3>

          <!-- Background -->
          <div class="prop-section">
            <label class="prop-label">Background</label>
            <el-select
              :model-value="selectedCard.bg"
              @update:model-value="setBg($event as BgType)"
              style="width: 100%"
            >
              <el-option
                v-for="opt in bgOptions"
                :key="opt.value"
                :label="opt.label"
                :value="opt.value"
              />
            </el-select>
          </div>

          <!-- Foregrounds (disabled for water_ripple/game_of_life/hourglass) -->
          <div class="prop-section" v-if="!isNoFg">
            <div class="prop-label-row">
              <label class="prop-label">Foregrounds ({{ selectedCard.fg_count }}/{{ MAX_FG_PER_CARD }})</label>
              <el-button size="small" text type="primary" @click="addForeground" :disabled="selectedCard.fg_count >= MAX_FG_PER_CARD">
                + Add
              </el-button>
            </div>

            <div v-if="selectedCard.fgs.length === 0" class="prop-hint">
              No foregrounds configured
            </div>

            <div
              v-for="(fg, fgIdx) in selectedCard.fgs"
              :key="fgIdx"
              class="fg-item"
            >
              <div class="fg-header">
                <span class="fg-index">FG {{ fgIdx + 1 }}</span>
                <div class="fg-actions">
                  <el-button size="small" text @click="moveFg(fgIdx, fgIdx - 1)" :disabled="fgIdx === 0">
                    <el-icon><ArrowUp /></el-icon>
                  </el-button>
                  <el-button size="small" text @click="moveFg(fgIdx, fgIdx + 1)" :disabled="fgIdx === selectedCard.fgs.length - 1">
                    <el-icon><ArrowDown /></el-icon>
                  </el-button>
                  <el-button size="small" text type="danger" @click="removeForeground(fgIdx)">
                    <el-icon><Delete /></el-icon>
                  </el-button>
                </div>
              </div>

              <div class="fg-fields">
                <el-form-item label="Type" label-position="top">
                  <el-select v-model="fg.type" @change="setFgType(fgIdx, fg.type)" style="width: 100%">
                    <el-option
                      v-for="(label, key) in FG_LABELS"
                      :key="key"
                      :label="label"
                      :value="key"
                    />
                  </el-select>
                </el-form-item>

                <!-- Font selector: disabled for Pong/Weather, locked to 3x5 for Fire -->
                <el-form-item
                  v-if="!isSimplified"
                  label="Font"
                  label-position="top"
                >
                  <el-select
                    v-model="fg.font"
                    :disabled="selectedCard.bg === 'fire'"
                    style="width: 100%"
                  >
                    <el-option
                      v-for="(label, key) in FONT_SIZES"
                      :key="key"
                      :label="label"
                      :value="key"
                    />
                  </el-select>
                </el-form-item>

                <el-form-item label="Duration (ms)" label-position="top">
                  <el-input-number
                    v-model="fg.duration_ms"
                    :min="1000"
                    :max="30000"
                    :step="1000"
                    style="width: 100%"
                  />
                </el-form-item>
              </div>
            </div>
          </div>

          <!-- No foregrounds note -->
          <div class="prop-section" v-else>
            <div class="prop-note">
              <el-icon><InfoFilled /></el-icon>
              <span>This background does not support foregrounds</span>
            </div>
          </div>

          <!-- Relative Brightness -->
          <div class="prop-section">
            <label class="prop-label">
              Relative Brightness:
              <strong>{{ selectedCard.relative_brightness }}</strong>
              ({{ selectedCard.relative_brightness < 0 ? 'darker' : selectedCard.relative_brightness > 0 ? 'brighter' : 'neutral' }})
            </label>
            <el-slider
              v-model="selectedCard.relative_brightness"
              :min="-128"
              :max="127"
              :marks="{ '-128': '-128', '0': '0', '127': '+127' }"
              show-input
            />
          </div>

          <!-- Auto-Switch Interval -->
          <div class="prop-section">
            <label class="prop-label">Auto-Switch Interval</label>
            <div class="prop-hint">Seconds until auto-advance to next card (0 = manual only)</div>
            <el-input-number
              v-model="selectedCard.switch_interval"
              :min="0"
              :max="300"
              :step="5"
              style="width: 100%"
            />
          </div>
        </div>

        <!-- No selection placeholder -->
        <div class="props-panel props-empty" v-else>
          <div class="empty-hint">
            <el-icon :size="40"><Select /></el-icon>
            <p>Select a card slot to edit</p>
          </div>
        </div>
      </div>

      <!-- Save Bar -->
      <div class="save-bar">
        <el-button type="primary" :loading="saving" @click="saveCards" size="large">
          <el-icon><Check /></el-icon>
          Save & Push to Device
        </el-button>
      </div>
    </template>

    <!-- No device -->
    <div v-else-if="!loading && !error" class="error-state">
      Device not found
    </div>
  </div>
</template>

<style scoped>
.card-editor {
  max-width: 1400px;
  margin: 0 auto;
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.editor-header {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 16px;
  flex-wrap: wrap;
}

.header-left-col {
  display: flex;
  align-items: center;
  gap: 16px;
  flex-wrap: wrap;
}

.editor-header h2 {
  font-size: 20px;
  font-weight: 600;
  color: var(--app-text);
}

.device-meta {
  font-size: 12px;
  color: var(--app-text-dim);
  font-family: monospace;
}

.editor-body {
  display: grid;
  grid-template-columns: 1fr 340px;
  gap: 24px;
  min-height: 600px;
}

.panel-title {
  font-size: 14px;
  font-weight: 600;
  color: var(--app-text-dim);
  text-transform: uppercase;
  letter-spacing: 0.5px;
  margin-bottom: 16px;
}

/* ── Card Slots Grid ─────────────────────────────────── */

.card-slots-panel {
  background: var(--app-surface);
  border: 1px solid var(--app-border);
  border-radius: 8px;
  padding: 20px;
}

.slots-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 12px;
}

.slot {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 4px;
  padding: 16px 8px;
  border: 2px dashed var(--app-border);
  border-radius: 8px;
  cursor: pointer;
  transition: all 0.15s;
  user-select: none;
  min-height: 80px;
}

.slot:hover {
  border-color: var(--app-accent);
  background: var(--app-surface-hover);
}

.slot-selected {
  border-color: var(--app-accent);
  background: rgba(108, 92, 231, 0.1);
  border-style: solid;
}

.slot-occupied {
  border-style: solid;
  border-color: var(--app-border);
}

.slot-number {
  font-size: 11px;
  color: var(--app-text-dim);
  font-weight: 700;
}

.slot-bg {
  font-size: 13px;
  color: var(--app-text);
  font-weight: 600;
  text-align: center;
}

.slot-detail {
  font-size: 11px;
  color: var(--app-text-dim);
}

/* ── Properties Panel ────────────────────────────────── */

.props-panel {
  background: var(--app-surface);
  border: 1px solid var(--app-border);
  border-radius: 8px;
  padding: 20px;
  overflow-y: auto;
}

.props-empty {
  display: flex;
  align-items: center;
  justify-content: center;
}

.empty-hint {
  text-align: center;
  color: var(--app-text-dim);
}

.empty-hint p {
  margin-top: 8px;
  font-size: 14px;
}

.prop-section {
  margin-bottom: 20px;
  padding-bottom: 20px;
  border-bottom: 1px solid var(--app-border);
}

.prop-section:last-child {
  border-bottom: none;
  margin-bottom: 0;
  padding-bottom: 0;
}

.prop-label {
  display: block;
  font-size: 13px;
  color: var(--app-text);
  font-weight: 600;
  margin-bottom: 8px;
}

.prop-label-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 8px;
}

.prop-label-row .prop-label {
  margin-bottom: 0;
}

.prop-hint {
  font-size: 11px;
  color: var(--app-text-dim);
  margin-bottom: 8px;
}

.prop-note {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 12px;
  background: rgba(253, 203, 110, 0.1);
  border-radius: 6px;
  font-size: 13px;
  color: var(--app-warning);
}

/* ── Foreground Items ────────────────────────────────── */

.fg-item {
  background: var(--app-bg);
  border: 1px solid var(--app-border);
  border-radius: 6px;
  padding: 12px;
  margin-bottom: 8px;
}

.fg-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 8px;
}

.fg-index {
  font-size: 12px;
  font-weight: 700;
  color: var(--app-accent-light);
}

.fg-actions {
  display: flex;
  gap: 4px;
}

.fg-fields {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.fg-fields :deep(.el-form-item) {
  margin-bottom: 8px;
}

.fg-fields :deep(.el-form-item__label) {
  font-size: 11px;
  padding-bottom: 2px;
}

/* ── Save Bar ────────────────────────────────────────── */

.save-bar {
  display: flex;
  justify-content: flex-end;
  padding: 16px 0;
}

.error-state {
  text-align: center;
  color: var(--app-danger);
  font-size: 16px;
  padding: 60px;
}
</style>
