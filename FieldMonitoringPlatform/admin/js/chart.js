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
            axisPointer: { type: 'line' },
            formatter: function (params) {
                // params 是当前鼠标悬浮点的数据数组
                if (!params || params.length === 0) return '';
                
                // params[0].value[0] 就是那串巨大的毫秒时间戳数字
                let date = new Date(params[0].value[0]);
                let Y = date.getFullYear();
                let M = date.getMonth() + 1;
                let d = date.getDate();
                let h = date.getHours().toString().padStart(2, '0');
                let m = date.getMinutes().toString().padStart(2, '0');
                let s = date.getSeconds().toString().padStart(2, '0');
                
                // 拼接出你想要的精美时间格式
                let timeStr = `${Y}年${M}月${d}日 ${h}:${m}:${s}`;
                
                // 构造弹出框的 HTML
                let html = `<div style="font-size:0.9rem; color:#666; margin-bottom:5px;">${timeStr}</div>`;
                params.forEach(param => {
                    // param.marker 是对应颜色的圆点，param.value[1] 是 Y轴 的数值
                    let val = param.value[1];
                    // 如果数值有小数，保留一位小数，更美观
                    if (typeof val === 'number' && !Number.isInteger(val)) val = val.toFixed(1);
                    
                    html += `<div>${param.marker} ${param.seriesName}: <span style="font-weight:bold; color:#333;">${val}</span></div>`;
                });
                return html;
            }
        },
        xAxis: { 
            type: 'value', // ✅ 核心降维打击：从 'time' 改成 'value' 纯数值轴
            min: timeRange.min, 
            max: timeRange.max, 
            interval: timeRange.interval, // 现在 ECharts 必须严格执行这个间隔，不敢少画一根线
            axisTick: { show: false }, 
            axisLine: { show: false },
            axisLabel: {
                showMinLabel: true, // 强制显示最左侧刻度
                showMaxLabel: true, // 强制显示最右侧刻度
                formatter: function (value) {
                    // 因为是 value 轴，传进来的是纯数字毫秒数，我们需要自己把它转换成时间文本
                    let date = new Date(value);
                    let M = (date.getMonth() + 1).toString().padStart(2, '0');
                    let d = date.getDate().toString().padStart(2, '0');
                    let h = date.getHours().toString().padStart(2, '0');
                    let m = date.getMinutes().toString().padStart(2, '0');
                    
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
    
    let ripeCount = 0;
    
    if(wmList.length === 0) {
        document.getElementById('wm-summary').innerText = `总计: 0 | 已熟: 0`;
        return;
    }

    // 获取当前时间戳 (秒)
    const currentUnixTime = Math.floor(new Date().getTime() / 1000);
    const OFFLINE_THRESHOLD = 30 * 60; // 30 分钟 (1800秒)

    wmList.forEach(wm => {
        let matPercent = Math.round(wm.maturity_score * 100);
        const isRipe = wm.maturity_score >= 1.0;
        if (isRipe) ripeCount++;
        
        // ✅ 核心判断：检查是否超过 30 分钟未更新
        // (兼容处理：如果后端没传 collected_at，默认为在线)
        let isOffline = false;
        if (wm.collected_at) {
            isOffline = (currentUnixTime - wm.collected_at) > OFFLINE_THRESHOLD;
        }
        
        const card = document.createElement('div');
        // 如果掉线，追加 offline 专属类名
        card.className = `wm-item ${isOffline ? 'offline' : ''}`;
        
        // 动态生成内部 HTML
        let htmlContent = `
            <div class="wm-id">${wm.device_id}</div>
            <div class="wm-status ${isRipe ? 'status-ripe' : 'status-unripe'}">${isRipe ? '成熟' : '生长'}(${matPercent}%)</div>
            <div class="wm-data">${wm.sugar_brix.toFixed(1)} <small>Brix</small></div>
        `;
        
        // 如果掉线，在卡片底部追加一个闪烁的红色警告
        if (isOffline) {
            htmlContent += `<span class="wm-offline-alert">⚠️ 超时未更新</span>`;
        }
        
        card.innerHTML = htmlContent;

        card.onclick = () => {
            document.querySelectorAll('.wm-item').forEach(el => el.classList.remove('active'));
            card.classList.add('active');
            window.currentSelectedWmId = wm.device_id;
            if(window.reloadAllChartsData) window.reloadAllChartsData();
            if(window.resetIdleTimer) window.resetIdleTimer();
        };
        container.appendChild(card);
    });
    
    document.getElementById('wm-summary').innerText = `总计: ${wmList.length} | 已熟: ${ripeCount}`;
}