import {
  Chart,
  LineController, LineElement, PointElement, LinearScale,
  TimeScale, Filler, Legend, Tooltip,
} from 'chart.js'
import 'chartjs-adapter-date-fns'

Chart.register(LineController, LineElement, PointElement, LinearScale,
               TimeScale, Filler, Legend, Tooltip)

const DEFAULTS = {
  responsive: true,
  maintainAspectRatio: true,
  aspectRatio: 3.5,
  interaction: { mode: 'index', intersect: false },
  plugins: {
    legend: {
      labels: { color: '#8892a4', boxWidth: 12, padding: 16 },
    },
    tooltip: {
      backgroundColor: '#1a1d27',
      borderColor: '#2a2d3a',
      borderWidth: 1,
      titleColor: '#e2e8f0',
      bodyColor: '#8892a4',
    },
  },
  scales: {
    x: {
      type: 'time',
      time: { tooltipFormat: 'MMM d, HH:mm' },
      ticks: { color: '#8892a4', maxTicksLimit: 8 },
      grid:  { color: '#2a2d3a' },
    },
    y: {
      ticks: { color: '#8892a4' },
      grid:  { color: '#2a2d3a' },
    },
  },
}

function line(color, label, data) {
  return {
    label,
    data,
    borderColor: color,
    backgroundColor: color + '22',
    borderWidth: 2,
    pointRadius: 0,
    tension: 0.3,
    fill: false,
  }
}

function makeChart(id, options) {
  const ctx = document.getElementById(id)
  return new Chart(ctx, { type: 'line', ...options })
}

export function createCharts() {
  const temp = makeChart('chart-temp', {
    data: { datasets: [
      line('#38bdf8', 'BMP280',  []),
      line('#c084fc', 'AHT20',   []),
      line('#4ade80', 'Average', []),
    ]},
    options: { ...DEFAULTS },
  })

  const press = makeChart('chart-press', {
    data: { datasets: [ line('#fb923c', 'Pressure', []) ] },
    options: { ...DEFAULTS },
  })

  const hum = makeChart('chart-hum', {
    data: { datasets: [ line('#4ade80', 'Humidity', []) ] },
    options: { ...DEFAULTS },
  })

  return { temp, press, hum }
}

export function updateCharts(charts, samples) {
  const bmpT = [], ahtT = [], avgT = [], press = [], hum = []

  for (const s of samples) {
    const t = s.timestamp
    if (s.bmp_temp_c != null) bmpT.push({ x: t, y: s.bmp_temp_c })
    if (s.aht_temp_c != null) ahtT.push({ x: t, y: s.aht_temp_c })

    const both = s.bmp_temp_c != null && s.aht_temp_c != null
    const avg  = both ? (s.bmp_temp_c + s.aht_temp_c) / 2
                      : (s.bmp_temp_c ?? s.aht_temp_c)
    if (avg != null) avgT.push({ x: t, y: +avg.toFixed(2) })

    if (s.bmp_press_hpa != null) press.push({ x: t, y: s.bmp_press_hpa })
    if (s.aht_hum_pct   != null) hum.push(  { x: t, y: s.aht_hum_pct   })
  }

  charts.temp.data.datasets[0].data = bmpT
  charts.temp.data.datasets[1].data = ahtT
  charts.temp.data.datasets[2].data = avgT
  charts.temp.update()

  charts.press.data.datasets[0].data = press
  charts.press.update()

  charts.hum.data.datasets[0].data = hum
  charts.hum.update()
}
