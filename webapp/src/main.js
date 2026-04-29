import { fetchSamples } from './data.js'
import { createCharts, updateCharts } from './charts.js'

const rangeEl      = document.getElementById('range')
const lastUpdated  = document.getElementById('last-updated')
const cardTemp     = document.getElementById('card-temp')
const cardPress    = document.getElementById('card-press')
const cardHum      = document.getElementById('card-hum')

const charts = createCharts()

function updateCards(samples) {
  const last = samples.at(-1)
  if (!last) return

  const bmp = last.bmp_temp_c
  const aht = last.aht_temp_c
  const avg = bmp != null && aht != null ? (bmp + aht) / 2
            : bmp != null ? bmp : aht

  cardTemp.textContent  = avg   != null ? avg.toFixed(1)              : '—'
  cardPress.textContent = last.bmp_press_hpa != null ? last.bmp_press_hpa.toFixed(1) : '—'
  cardHum.textContent   = last.aht_hum_pct   != null ? last.aht_hum_pct.toFixed(1)   : '—'
}

async function refresh() {
  const hours = Number(rangeEl.value)
  const samples = await fetchSamples(hours)
  updateCards(samples)
  updateCharts(charts, samples)
  lastUpdated.textContent = 'Updated ' + new Date().toLocaleTimeString()
}

rangeEl.addEventListener('change', refresh)
refresh()
setInterval(refresh, 60_000)
