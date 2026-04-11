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

// 通用配置生成器：支持连续时间轴 + 强制精确刻度 + 红色标记点
function buildStrictOption(data, min, max, interval, colorsArray, timeRange, threshold = null) {
    let option = {
        grid: { left: '8%', right: '4%', bottom: '15%', top: '10%' },
        tooltip: { 
            trigger: 'axis',
            axisPointer: { type: 'line' }
        },
        xAxis: { 
            type: 'time', 
            min: timeRange.min, 
            max: timeRange.max, 
            interval: timeRange.interval, // ✅ 核心修复：强行夺回控制权，强制使用我们计算好的毫秒间隔
            axisTick: { show: false }, 
            axisLine: { show: false },
            axisLabel: {
                hideOverlap: true, // 防止极端小屏幕下的文字重叠
                formatter: function (value) {
                    let date = new Date(value);
                    let M = (date.getMonth() + 1).toString().padStart(2, '0');
                    let d = date.getDate().toString().padStart(2, '0');
                    let h = date.getHours().toString().padStart(2, '0');
                    let m = date.getMinutes().toString().padStart(2, '0');
                    
                    // 根据毫秒间隔判断显示 时:分 还是 月.日
                    if (timeRange.interval >= 24 * 3600 * 1000) {
                        return `${M}.${d}`; 
                    } else {
                        return `${h}:${m}`; 
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
            data: data, 
            smooth: false,
            showSymbol: true,          // ✅ 核心修复1：显示数据点
            symbol: 'circle',          // ✅ 核心修复2：设置为圆形
            symbolSize: 6,             // 点的大小
            itemStyle: { color: '#e74c3c' }, // ✅ 核心修复3：红色数据点
            lineStyle: { color: '#2c3e50', width: 2.5 }, // 深灰色折线
            connectNulls: false
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