// ==== 基础数据 二级菜单（IE7 兼容）====
var subMenuTimer = null;

function toggleBasicDataMenu() {
    var sub = document.getElementById('basicDataSubmenu');
    if (!sub) return;
    sub.style.display = (sub.style.display === 'block') ? 'none' : 'block';
}

function hideBasicDataMenu() {
    var sub = document.getElementById('basicDataSubmenu');
    if (sub) sub.style.display = 'none';
    subMenuTimer = null;
}

// 鼠标进入有效区（磁贴+二级菜单）：取消隐藏定时器
function onSubMenuMouseOver() {
    if (subMenuTimer) { clearTimeout(subMenuTimer); subMenuTimer = null; }
}

// 鼠标离开有效区：延迟200ms隐藏（磁贴→子菜单的移动会先out后over，定时器被取消，不会误关）
function onSubMenuMouseOut() {
    if (subMenuTimer) clearTimeout(subMenuTimer);
    subMenuTimer = setTimeout(hideBasicDataMenu, 200);
}

function onBasicDataCmd(cmd) {
    hideBasicDataMenu();
    onMenuCmd(cmd);   // 复用现有分发：window.external.OnJsCommand(cmd, "")
}