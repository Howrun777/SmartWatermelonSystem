// admin/js/auto_switch.js

let currentFieldIndex = 0;
let fieldList = [];
let idleTimer = null;
let dataRefreshTimer = null; // ✅ 新增：专门用于后台静默刷新数据的定时器
const IDLE_TIMEOUT = 60000;  // 60秒无操作切换瓜田
const REFRESH_RATE = 6000;   // ✅ 新增：每 6 秒后台偷偷拉取一次最新数据

const SCALE_NAMES = ["5分钟", "2小时", "1天", "1周"];
let scales = { sugar: 0, temp: 0, hum: 0, light: 0 };

window.currentSelectedWmId = null;
window.currentFieldId = null;

document.addEventListener('DOMContentLoaded', async () => {
    // 监听用户交互
    document.addEventListener('mousemove', resetIdleTimer);
    document.addEventListener('mousedown', resetIdleTimer);
    document.addEventListener('keypress', resetIdleTimer);

    await fetchFieldList();
    if (fieldList.length > 0) {
        loadFieldData(currentFieldIndex);
        startAutoSwitch();
        startDataRefresh(); // ✅ 启动后台静默刷新
    } else {
        document.getElementById('field-info-display').innerHTML = `<span style="color:red;">未查询到任何瓜田信息</span>`;
    }
});

// ====== 基础控制逻辑 ======

// ✅ 用户点击“强制刷新”按钮
window.forceRefresh = function() {
    console.log("手动强制刷新数据！");
    resetIdleTimer();
    // 重新拉取当前瓜田和西瓜数据
    loadFieldData(currentFieldIndex, true);
}

// 更改时间刻度
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

// 切换瓜田
function switchField(step) {
    if (fieldList.length === 0) return;
    resetIdleTimer();
    currentFieldIndex = (currentFieldIndex + step + fieldList.length) % fieldList.length;
    window.currentSelectedWmId = null; 
    loadFieldData(currentFieldIndex);
}

// ====== 定时器管理 ======

function resetIdleTimer() { 
    clearTimeout(idleTimer); 
    startAutoSwitch(); 
}

// 控制大屏切换的宏观定时器 (比如 60 秒不动就切下一个大棚)
function startAutoSwitch() { 
    idleTimer = setTimeout(() => { switchField(1); }, IDLE_TIMEOUT); 
}

// ✅ 控制数据刷新的微观定时器 (永远在后台 6 秒跑一次)
function startDataRefresh() {
    clearInterval(dataRefreshTimer);
    dataRefreshTimer = setInterval(() => {
        // 如果当前有选中的瓜田，就在后台悄悄拉取数据更新图表，不触发闪烁动画
        if (window.currentFieldId) {
            window.reloadAllChartsData();
        }
    }, REFRESH_RATE);
}


// ====== 网络请求与数据处理 ======

async function fetchFieldList() {
    try {
        const res = await fetch('/api/admin/field/list');
        const json = await res.json();
        if (json.code === 200) fieldList = json.data.list.map(item => item.field_id);
    } catch (e) { console.error("获取列表失败", e); }
}

// loadFieldData 增加了一个 isForce 参数，防止强制刷新时重置你选中的西瓜
async function loadFieldData(index, isForce = false) {
    const fieldId = fieldList[index];
    window.currentFieldId = fieldId;
    document.getElementById('field-info-display').innerHTML = `当前查看：<span style="color:#f1c40f;">瓜田 ${fieldId}</span> (${index + 1}/${fieldList.length})`;

    try {
        const resWm = await fetch(`/api/admin/watermelon/list?field_id=${fieldId}`);
        const jsonWm = await resWm.json();
        if (jsonWm.code === 200) {
            renderWatermelonGrid(jsonWm.data);
            
            // 只有在非强制刷新，或者当前没有选中西瓜时，才默认选中第一个
            if (jsonWm.data.length > 0 && (!window.currentSelectedWmId || !isForce)) {
                window.currentSelectedWmId = jsonWm.data[0].device_id;
            }
            
            // 重新挂载高亮样式
            setTimeout(() => { 
                const cards = document.querySelectorAll('.wm-item');
                cards.forEach(card => {
                    if (card.querySelector('.wm-id').innerText === window.currentSelectedWmId) {
                        card.classList.add('active');
                    }
                });
            }, 50);
        }
        window.reloadAllChartsData();
    } catch (e) { console.error("加载数据失败", e); }
}

// 根据刻度级别，计算当前的 X 轴边界
function getTimeRange(scaleLevel) {
    let now = new Date().getTime(); 
    let interval = 0;
    
    if (scaleLevel === 0) interval = 5 * 60 * 1000;              // 5分钟
    else if (scaleLevel === 1) interval = 2 * 60 * 60 * 1000;    // 2小时
    else if (scaleLevel === 2) interval = 24 * 60 * 60 * 1000;   // 1天
    else if (scaleLevel === 3) interval = 7 * 24 * 60 * 60 * 1000; // 1周
    
    let past = now - (11 * interval); 
    return { min: past, max: now, interval: interval };
}

window.reloadAllChartsData = async function() {
    if (!window.currentFieldId) return;

    try {
        const resEnv = await fetch(`/api/admin/field/environment?field_id=${window.currentFieldId}`);
        const jsonEnv = await resEnv.json();
        
        if (jsonEnv.code === 200) {
            const rawData = jsonEnv.data;
            const tempData = rawData.map(d => [d.timestamp * 1000, d.temperature]);
            const humData = rawData.map(d => [d.timestamp * 1000, d.humidity]);
            const lightData = rawData.map(d => [d.timestamp * 1000, d.light]);
            
            const tempRange = getTimeRange(scales.temp);
            const humRange = getTimeRange(scales.hum);
            const lightRange = getTimeRange(scales.light);

            renderEnvHistoryCharts(tempData, humData, lightData, tempRange, humRange, lightRange);
        }

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