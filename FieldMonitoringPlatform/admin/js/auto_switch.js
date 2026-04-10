// admin/js/auto_switch.js

let currentFieldIndex = 0;
let fieldList = []; // 不再写死，从服务器获取
let idleTimer = null;
const IDLE_TIMEOUT = 60000; 

const SCALE_NAMES = ["5分钟", "2小时", "1天", "1周"];
let scales = { sugar: 0, temp: 0, hum: 0, light: 0 };

window.currentSelectedWmId = null;
window.currentFieldId = null;

// ====== 页面初始化 ======
document.addEventListener('DOMContentLoaded', async () => {
    document.addEventListener('mousemove', resetIdleTimer);
    document.addEventListener('mousedown', resetIdleTimer);
    document.addEventListener('keypress', resetIdleTimer);

    // 1. 获取瓜田列表
    await fetchFieldList();
    
    // 2. 如果有瓜田，加载第一个，并启动轮询
    if (fieldList.length > 0) {
        loadFieldData(currentFieldIndex);
        startAutoSwitch();
    } else {
        document.getElementById('field-info-display').innerHTML = `<span style="color:red;">未查询到任何瓜田信息</span>`;
    }
});

// ====== 基础控制逻辑 ======
function changeScale(scaleKey, step) {
    resetIdleTimer();
    let current = scales[scaleKey];
    current += step;
    if (current < 0) current = 0;
    if (current > 3) current = 3;

    if (scales[scaleKey] !== current) {
        scales[scaleKey] = current;
        document.getElementById(`text-${scaleKey}`).innerText = `时间刻度: ${SCALE_NAMES[current]}`;
        window.reloadAllChartsData();
    }
}

function switchField(step) {
    if (fieldList.length === 0) return;
    resetIdleTimer();
    currentFieldIndex = (currentFieldIndex + step + fieldList.length) % fieldList.length;
    window.currentSelectedWmId = null; 
    loadFieldData(currentFieldIndex);
}

function resetIdleTimer() { clearTimeout(idleTimer); startAutoSwitch(); }
function startAutoSwitch() { idleTimer = setTimeout(() => { switchField(1); }, IDLE_TIMEOUT); }

// ====== 网络请求与数据处理 ======

// 获取所有的瓜田号
async function fetchFieldList() {
    try {
        const res = await fetch('/api/admin/field/list');
        const json = await res.json();
        if (json.code === 200) {
            // 提取所有的 field_id 存入数组
            fieldList = json.data.list.map(item => item.field_id);
        }
    } catch (e) { console.error("获取瓜田列表失败", e); }
}

// 加载指定瓜田的数据
async function loadFieldData(index) {
    const fieldId = fieldList[index];
    window.currentFieldId = fieldId;
    
    document.getElementById('field-info-display').innerHTML = 
        `当前查看：<span style="color:#f1c40f;">瓜田 ${fieldId}</span> (${index + 1}/${fieldList.length})`;

    try {
        // 请求1：获取此瓜田下的西瓜阵列
        const resWm = await fetch(`/api/admin/watermelon/list?field_id=${fieldId}`);
        const jsonWm = await resWm.json();
        if (jsonWm.code === 200) {
            renderWatermelonGrid(jsonWm.data);
            if (jsonWm.data.length > 0 && !window.currentSelectedWmId) {
                window.currentSelectedWmId = jsonWm.data[0].device_id;
                setTimeout(() => { document.querySelector('.wm-item').classList.add('active'); }, 50);
            }
        }
        
        // 渲染图表
        window.reloadAllChartsData();
    } catch (e) { console.error("加载瓜田数据失败", e); }
}


// ====== 数据二次处理引擎 (供图表调用) ======

// 生成 12 个 X 轴时间刻度标签
function generateTimeLabels(scaleLevel) {
    let labels = [];
    let now = new Date();
    for(let i=11; i>=0; i--) {
        let t = new Date(now.getTime());
        if(scaleLevel === 0) t.setMinutes(now.getMinutes() - i * 5);        
        else if(scaleLevel === 1) t.setHours(now.getHours() - i * 2);       
        else if(scaleLevel === 2) t.setDate(now.getDate() - i * 1);         
        else if(scaleLevel === 3) t.setDate(now.getDate() - i * 7);         
        
        if(scaleLevel <= 1) labels.push(`${String(t.getHours()).padStart(2,'0')}:${String(t.getMinutes()).padStart(2,'0')}`);
        else labels.push(`${String(t.getMonth()+1).padStart(2,'0')}.${String(t.getDate()).padStart(2,'0')}`);
    }
    return labels;
}

// 核心函数：根据后端传来的 7 天杂乱数据，提取并平均计算出对应 X 轴时间点的 Y 值
function processChartData(apiData, scaleLevel, dataKey) {
    let result = [];
    let nowStr = new Date();
    let now = Math.floor(nowStr.getTime() / 1000); // 转换为秒级时间戳
    
    // 确定时间间隔(秒)
    let intervalSeconds = 0;
    if(scaleLevel === 0) intervalSeconds = 5 * 60;
    else if(scaleLevel === 1) intervalSeconds = 2 * 3600;
    else if(scaleLevel === 2) intervalSeconds = 24 * 3600;
    else if(scaleLevel === 3) intervalSeconds = 7 * 24 * 3600;

    // 倒推 12 个时间段，匹配数据
    for(let i = 11; i >= 0; i--) {
        let targetTime = now - i * intervalSeconds;
        let endTime = targetTime + intervalSeconds;
        
        // 筛选落在这个时间段内的数据
        let matchedItems = apiData.filter(d => d.timestamp >= targetTime && d.timestamp < endTime);
        
        if (matchedItems.length > 0) {
            // 计算平均值 (或者取最后一个)
            let sum = matchedItems.reduce((acc, curr) => acc + curr[dataKey], 0);
            result.push(sum / matchedItems.length);
        } else {
            // 如果这个时间段没有数据，沿用上一个点，或者给个 0
            result.push(result.length > 0 ? result[result.length - 1] : 0);
        }
    }
    return result;
}

// 全局刷新图表
window.reloadAllChartsData = async function() {
    if (!window.currentFieldId) return;

    try {
        // 请求环境数据
        const resEnv = await fetch(`/api/admin/field/environment?field_id=${window.currentFieldId}`);
        const jsonEnv = await resEnv.json();
        if (jsonEnv.code === 200) {
            const rawData = jsonEnv.data;
            const tempTimes = generateTimeLabels(scales.temp);
            const humTimes = generateTimeLabels(scales.hum);
            const lightTimes = generateTimeLabels(scales.light);

            const tempData = processChartData(rawData, scales.temp, 'temperature');
            const humData = processChartData(rawData, scales.hum, 'humidity');
            const lightData = processChartData(rawData, scales.light, 'light');
            
            renderEnvHistoryCharts(tempTimes, tempData, humTimes, lightTimes);
        }

        // 请求单个西瓜糖度数据
        if (window.currentSelectedWmId) {
            const resWm = await fetch(`/api/admin/watermelon/history?device_id=${window.currentSelectedWmId}`);
            const jsonWm = await resWm.json();
            if (jsonWm.code === 200) {
                const sugarTimes = generateTimeLabels(scales.sugar);
                const sugarData = processChartData(jsonWm.data, scales.sugar, 'sugar_brix');
                renderWmHistoryChart(sugarTimes, sugarData, window.currentSelectedWmId, 12.5);
            }
        }
    } catch (e) {
        console.error("图表数据获取失败", e);
    }
}