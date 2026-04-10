// admin/js/auto_switch.js

let currentFieldIndex = 0;
let fieldList = [];
let idleTimer = null;
const IDLE_TIMEOUT = 60000; 

const SCALE_NAMES = ["5分钟", "2小时", "1天", "1周"];
let scales = { sugar: 0, temp: 0, hum: 0, light: 0 };

window.currentSelectedWmId = null;
window.currentFieldId = null;

document.addEventListener('DOMContentLoaded', async () => {
    document.addEventListener('mousemove', resetIdleTimer);
    document.addEventListener('mousedown', resetIdleTimer);
    document.addEventListener('keypress', resetIdleTimer);

    await fetchFieldList();
    if (fieldList.length > 0) {
        loadFieldData(currentFieldIndex);
        startAutoSwitch();
    } else {
        document.getElementById('field-info-display').innerHTML = `<span style="color:red;">未查询到任何瓜田信息</span>`;
    }
});

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

async function fetchFieldList() {
    try {
        const res = await fetch('/api/admin/field/list');
        const json = await res.json();
        if (json.code === 200) fieldList = json.data.list.map(item => item.field_id);
    } catch (e) { console.error("获取列表失败", e); }
}

async function loadFieldData(index) {
    const fieldId = fieldList[index];
    window.currentFieldId = fieldId;
    document.getElementById('field-info-display').innerHTML = `当前查看：<span style="color:#f1c40f;">瓜田 ${fieldId}</span> (${index + 1}/${fieldList.length})`;

    try {
        const resWm = await fetch(`/api/admin/watermelon/list?field_id=${fieldId}`);
        const jsonWm = await resWm.json();
        if (jsonWm.code === 200) {
            renderWatermelonGrid(jsonWm.data);
            if (jsonWm.data.length > 0 && !window.currentSelectedWmId) {
                window.currentSelectedWmId = jsonWm.data[0].device_id;
                setTimeout(() => { document.querySelector('.wm-item').classList.add('active'); }, 50);
            }
        }
        window.reloadAllChartsData();
    } catch (e) { console.error("加载数据失败", e); }
}

// ====== 核心修改：动态视口计算 ======

// 根据刻度级别，计算当前的 X 轴边界 (返回毫秒级时间戳)
function getTimeRange(scaleLevel) {
    let now = new Date().getTime(); 
    let past = now;
    
    // 刻度: 0=5min(视口1小时), 1=2h(视口24小时), 2=1d(视口12天), 3=1week(视口84天)
    if (scaleLevel === 0) past = now - (60 * 60 * 1000); 
    else if (scaleLevel === 1) past = now - (24 * 60 * 60 * 1000); 
    else if (scaleLevel === 2) past = now - (12 * 24 * 60 * 60 * 1000); 
    else if (scaleLevel === 3) past = now - (84 * 24 * 60 * 60 * 1000); 
    
    return { min: past, max: now };
}

window.reloadAllChartsData = async function() {
    if (!window.currentFieldId) return;

    try {
        // 请求环境数据 (获取所有84天的全量数据)
        const resEnv = await fetch(`/api/admin/field/environment?field_id=${window.currentFieldId}`);
        const jsonEnv = await resEnv.json();
        
        if (jsonEnv.code === 200) {
            const rawData = jsonEnv.data;
            
            // ✅ 直接提取数据为 ECharts 'time' 轴所需的 [[毫秒时间戳, 值], ...] 格式
            // 不做任何平均运算，保持绝对真实！
            const tempData = rawData.map(d => [d.timestamp * 1000, d.temperature]);
            const humData = rawData.map(d => [d.timestamp * 1000, d.humidity]);
            const lightData = rawData.map(d => [d.timestamp * 1000, d.light]);
            
            // 获取每个图表当前的视口范围
            const tempRange = getTimeRange(scales.temp);
            const humRange = getTimeRange(scales.hum);
            const lightRange = getTimeRange(scales.light);

            renderEnvHistoryCharts(tempData, humData, lightData, tempRange, humRange, lightRange);
        }

        // 请求单个西瓜糖度数据
        if (window.currentSelectedWmId) {
            const resWm = await fetch(`/api/admin/watermelon/history?device_id=${window.currentSelectedWmId}`);
            const jsonWm = await resWm.json();
            
            if (jsonWm.code === 200) {
                const sugarData = jsonWm.data.map(d => [d.timestamp * 1000, d.sugar_brix]);
                const sugarRange = getTimeRange(scales.sugar);
                renderWmHistoryChart(sugarData, window.currentSelectedWmId, sugarRange, 12.5);
            }
        }
    } catch (e) {
        console.error("图表渲染失败", e);
    }
}