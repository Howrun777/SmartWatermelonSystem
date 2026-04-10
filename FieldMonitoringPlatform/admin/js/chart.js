// admin/js/chart.js

let wmHistoryChart = null;
let tempChart = null;
let humChart = null;
let lightChart = null;

document.addEventListener('DOMContentLoaded', () => {
    wmHistoryChart = echarts.init(document.getElementById('wmHistoryChart'));
    tempChart = echarts.init(document.getElementById('tempChart'));
    humChart = echarts.init(document.getElementById('humChart'));
    lightChart = echarts.init(document.getElementById('lightChart'));

    window.addEventListener('resize', () => {
        wmHistoryChart.resize();
        tempChart.resize(); humChart.resize(); lightChart.resize();
    });
});

// 通用配置生成器：支持连续时间轴 (type: 'time')
function buildStrictOption(data, min, max, interval, colorsArray, timeRange, threshold = null) {
    let option = {
        grid: { left: '8%', right: '4%', bottom: '15%', top: '10%' },
        tooltip: { 
            trigger: 'axis',
            axisPointer: { type: 'line' }
        },
        xAxis: { 
            type: 'time', // ✅ 核心改变：连续时间轴
            min: timeRange.min, // 动态限制视口左边界
            max: timeRange.max, // 动态限制视口右边界
            splitNumber: 12,    // 期望显示 12 个刻度
            axisTick: { show: false }, 
            axisLine: { show: false },
            axisLabel: {
                formatter: function (value) {
                    // 根据视口跨度智能显示时间格式
                    let date = new Date(value);
                    let M = (date.getMonth() + 1).toString().padStart(2, '0');
                    let d = date.getDate().toString().padStart(2, '0');
                    let h = date.getHours().toString().padStart(2, '0');
                    let m = date.getMinutes().toString().padStart(2, '0');
                    
                    let durationHours = (timeRange.max - timeRange.min) / (1000 * 3600);
                    if (durationHours > 24) {
                        return `${M}.${d}`; // 天/周级别显示 月.日
                    } else {
                        return `${h}:${m}`; // 分钟/小时级别显示 时:分
                    }
                }
            }
        },
        yAxis: { 
            type: 'value', 
            min: min, max: max, interval: interval,
            axisLabel: {
                formatter: '{value}',
                color: (value) => {
                    let idx = Math.round((value - min) / interval);
                    return colorsArray[idx] || '#333';
                }
            },
            splitLine: { show: true, lineStyle: { type: 'solid', color: colorsArray } }
        },
        series: [{
            type: 'line',
            data: data, // ✅ 格式为 [[时间戳, 值], [时间戳, 值]...]
            smooth: false,
            showSymbol: false,
            connectNulls: false, // 数据断档时不强制连线
            lineStyle: { color: '#2c3e50', width: 2.5 }, // 你的深灰色线条
            itemStyle: { color: '#888' },
        }]
    };

    if (threshold !== null) {
        option.series[0].markLine = {
            symbol: 'none',
            data: [{ yAxis: threshold }],
            lineStyle: { color: '#f39c12', type: 'solid', width: 2 },
            label: { show: false }
        };
    }

    return option;
}

function renderWmHistoryChart(data, deviceId, timeRange, maturityThreshold = 12.5) {
    document.getElementById('wm-history-title').innerText = `[${deviceId}] 糖度历史变化`;
    const colors = ['#333', '#333', '#333', '#333', '#333', '#333', '#e74c3c', '#e74c3c', '#e74c3c'];
    wmHistoryChart.setOption(buildStrictOption(data, 0, 16, 2, colors, timeRange, maturityThreshold), true);
}

function renderEnvHistoryCharts(tempData, humData, lightData, tempRange, humRange, lightRange) {
    const tempColors = ['#e74c3c', '#e74c3c', '#333', '#333', '#333', '#e74c3c', '#e74c3c', '#e74c3c'];
    tempChart.setOption(buildStrictOption(tempData, -10, 60, 10, tempColors, tempRange), true);

    const humColors = ['#e74c3c', '#e74c3c', '#333', '#333', '#333', '#e74c3c', '#e74c3c'];
    humChart.setOption(buildStrictOption(humData, 30, 90, 10, humColors, humRange), true);

    const lightColors = ['#e74c3c', '#e74c3c', '#e74c3c', '#333', '#333', '#333', '#333', '#e74c3c', '#e74c3c'];
    lightChart.setOption(buildStrictOption(lightData, 0, 90000, 10000, lightColors, lightRange), true);
}

function renderWatermelonGrid(wmList) {
    const container = document.getElementById('wm-grid-container');
    container.innerHTML = '';
    if(wmList.length === 0) return;

    wmList.forEach(wm => {
        let matPercent = Math.round(wm.maturity_score * 100);
        const card = document.createElement('div');
        card.className = `wm-item`;
        card.innerHTML = `
            <div class="wm-id">${wm.device_id}</div>
            <div class="wm-status">成熟(${matPercent}%)</div>
            <div class="wm-data">${wm.sugar_brix.toFixed(1)} Brix</div>
        `;
        card.onclick = () => {
            document.querySelectorAll('.wm-item').forEach(el => el.classList.remove('active'));
            card.classList.add('active');
            window.currentSelectedWmId = wm.device_id;
            if(window.reloadAllChartsData) window.reloadAllChartsData();
            resetIdleTimer();
        };
        container.appendChild(card);
    });
}