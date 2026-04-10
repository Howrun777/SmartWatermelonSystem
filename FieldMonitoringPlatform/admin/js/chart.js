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

// 通用配置生成器：实现红黑刻度线 (彻底放弃默认自适应，强制固定坐标轴)
function buildStrictOption(xData, yData, min, max, interval, colorsArray, threshold = null) {
    let option = {
        grid: { left: '8%', right: '4%', bottom: '15%', top: '10%' },
        tooltip: { trigger: 'axis' },
        xAxis: { 
            type: 'category', 
            data: xData, 
            boundaryGap: false,
            axisTick: { show: false }, // 隐藏多余刻度小短线
            axisLine: { show: false }  // 隐藏底边横线
        },
        yAxis: { 
            type: 'value', 
            min: min, max: max, interval: interval,
            axisLabel: {
                formatter: '{value}',
                color: (value) => {
                    // 根据值在刻度数组中的索引决定文字颜色 (与线条颜色同步)
                    let idx = Math.round((value - min) / interval);
                    return colorsArray[idx] || '#333';
                }
            },
            splitLine: {
                show: true,
                lineStyle: {
                    type: 'solid',
                    color: colorsArray // 传入颜色数组，ECharts会从下往上一条条画
                }
            }
        },
        /*
        series: [{
            type: 'line', 
            data: yData, 
            smooth: false, // 参考图线条是直线的折线
            showSymbol: false, // 隐藏折线上的小圆点
            lineStyle: { color: 'rgba(0,0,0,0)', width: 0 }, // 隐藏折线本身，只留填充色？不，保留线条
            itemStyle: { color: '#888' },
        }]
        */
        series: [{
            type: 'line',
            data: yData,
            smooth: false,
            showSymbol: false,
            lineStyle: { color: '#2c3e50', width: 2.5 },   // ✅ 改成可见的颜色和线宽
            itemStyle: { color: '#888' },
        }]
        
    };

    /*如果有糖度成熟标准线 (黄色标注)
    if (threshold !== null) {
        option.series[0].markLine = {
            symbol: 'none',
            data: [{ yAxis: threshold }],
            lineStyle: { color: '#f39c12', type: 'solid', width: 2 },
            label: { show: false } // 不显示右侧文字
        };
    }
    */

    return option;
}

// 1. 渲染糖度图表
function renderWmHistoryChart(xData, yData, deviceId, maturityThreshold = 12.5) {
    document.getElementById('wm-history-title').innerText = `[${deviceId}] 糖度历史变化`;
    // 0~16, 间隔2. 颜色映射：0~10黑, 12~16红
    const colors = ['#333', '#333', '#333', '#333', '#333', '#333', '#e74c3c', '#e74c3c', '#e74c3c'];
    wmHistoryChart.setOption(buildStrictOption(xData, yData, 0, 16, 2, colors, maturityThreshold), true);
}

// 2. 渲染三个环境图表
function renderEnvHistoryCharts(xData, tempData, humData, lightData) {
    // 温度: -10~60, 间隔10. 红(-10,0) 黑(10,20,30) 红(40,50,60)
    const tempColors = ['#e74c3c', '#e74c3c', '#333', '#333', '#333', '#e74c3c', '#e74c3c', '#e74c3c'];
    tempChart.setOption(buildStrictOption(xData, tempData, -10, 60, 10, tempColors), true);

    // 湿度: 30~90, 间隔10. 红(30,40) 黑(50,60,70) 红(80,90)
    const humColors = ['#e74c3c', '#e74c3c', '#333', '#333', '#333', '#e74c3c', '#e74c3c'];
    humChart.setOption(buildStrictOption(xData, humData, 30, 90, 10, humColors), true);

    // 光照: 10000~90000, 间隔10000. 红(1w,2w,3w) 黑(4w,5w,6w,7w) 红(8w,9w)
    const lightColors = ['#e74c3c', '#e74c3c', '#e74c3c', '#333', '#333', '#333', '#333', '#e74c3c', '#e74c3c'];
    lightChart.setOption(buildStrictOption(xData, lightData, 10000, 90000, 10000, lightColors), true);
}

// 3. 渲染西瓜阵列
function renderWatermelonGrid(wmList) {
    const container = document.getElementById('wm-grid-container');
    container.innerHTML = '';
    
    if(wmList.length === 0) return;

    wmList.forEach(wm => {
        // 成熟度换算百分比
        let matPercent = Math.round(wm.maturity_score * 100);
        
        const card = document.createElement('div');
        card.className = `wm-item`;
        // 渲染与UI图一致的卡片
        card.innerHTML = `
            <div class="wm-id">${wm.device_id}</div>
            <div class="wm-status">成熟(${matPercent}%)</div>
            <div class="wm-data">${wm.sugar_brix.toFixed(1)} Brix</div>
        `;
        
        card.onclick = () => {
            document.querySelectorAll('.wm-item').forEach(el => el.classList.remove('active'));
            card.classList.add('active');
            
            // 记录选中的西瓜ID，供数据刷新使用
            window.currentSelectedWmId = wm.device_id;
            // 触发数据重载
            if(window.reloadAllChartsData) window.reloadAllChartsData();
            
            resetIdleTimer();
        };
        container.appendChild(card);
    });
}