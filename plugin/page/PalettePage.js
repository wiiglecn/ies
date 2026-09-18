var chatMessages = []; 
var lastMessage = {
    date: new Date(),
    text: "AI 思考中.",
    lastMessageDiv:null
}
var blinkInterval = null;
var typingInterval = null; // 新增：打字机效果的定时器
var waitingForReply = false;
var lastUserMessage = "";

if (!window.console) {
    window.console = {
        log: function() {},
        error: function() {},
        warn: function() {},
        info: function() {}
    };
}
if (!window.JSON) {
    window.JSON = {
        parse: function(sJson) {
            return eval("(" + sJson + ")");
        },
        stringify: function(value) {
            if (value === null) return "null";
            if (value === undefined || typeof value === "function") return undefined;
            var type = typeof value;
            if (type === "number" || type === "boolean") return String(value);
            if (type === "string") return '"' + escapeString(value) + '"';
            if (type === "object") {
                if (value instanceof Date) return '"' + value.toISOString() + '"';
                if (value instanceof Array) {
                    var arrResult = [];
                    for (var i = 0; i < value.length; i++) {
                        var itemStr = JSON.stringify(value[i]);
                        if (itemStr === undefined) arrResult.push("null");
                        else arrResult.push(itemStr);
                    }
                    return "[" + arrResult.join(",") + "]";
                }
                var objResult = [];
                for (var key in value) {
                    if (value.hasOwnProperty(key)) {
                        var valStr = JSON.stringify(value[key]);
                        if (valStr !== undefined) {
                            objResult.push('"' + escapeString(key) + '":' + valStr);
                        }
                    }
                }
                return "{" + objResult.join(",") + "}";
            }
        }
    };

    function escapeString(str) {
        return str.replace(/["\\\x00-\x1f\x7f-\x9f]/g, function(match) {
            switch (match) {
                case "\"": return "\\\"";
                case "\\": return "\\\\";
                case "\b": return "\\b";
                case "\f": return "\\f";
                case "\n": return "\\n";
                case "\r": return "\\r";
                case "\t": return "\\t";
                default:
                    return "\\u" + ("0000" + match.charCodeAt(0).toString(16)).slice(-4);
            }
        });
    }
}
// Helper function for IE7 compatibility to trim strings
function trimString(str) {
    if (!str) return "";
    return str.replace(/^\s+|\s+$/g, '');
}


// Helper function for IE7 compatibility
function getElementsByClass(searchClass, node, tag) {
    node = node || document;
    tag = tag || "*";
    var elements = node.getElementsByTagName(tag);
    var result = [];
    for (var i = 0; i < elements.length; i++) {
        if (elements[i].className.indexOf(searchClass) !== -1) {
            result.push(elements[i]);
        }
    }
    return result;
}

// Debounce variable
var resizeTimeout = null;

function resizeLayout() {

    // Use debounce to prevent rapid firing
    if (resizeTimeout) clearTimeout(resizeTimeout);

    var groupAi = document.getElementById('group-ai');
    var groupContent = null;
    // Option 1: Using your existing helper function (Recommended for IE7)
    var contents = getElementsByClass('group-content', groupAi, 'div');
    if (contents.length > 0) {
        groupContent = contents[0];
    }
    
    // If the content is collapsed, do not resize/return early
    if (groupContent && groupContent.className.indexOf('collapsed') !== -1) {
        resizeTimeout = null;
        groupAi.style.height = "";
        return;
    }
    
    resizeTimeout = setTimeout(function() {
        var bodyHeight = document.body.clientHeight;
        if (bodyHeight < 600) return;
        var groupProbe = document.getElementById('group-probe');
        
        if (!groupProbe || !groupAi) return;

        // Calculate height of the first group including margins
        var probeHeight = groupProbe.offsetHeight; 
        var totalUsedHeight = probeHeight + 10; // margin-bottom
        
        // Calculate remaining height
        // Body padding is 10px top + 10px bottom = 20px
        var availableHeight = bodyHeight - totalUsedHeight - 40;

        if (availableHeight < 100) availableHeight = 100;


        // Fix: Handle empty string or NaN from style.height
        var currentAiHeight = parseInt(groupAi.style.height, 10);
        if (isNaN(currentAiHeight)) {
            currentAiHeight = -1; // Force update on first run
        }
        // Only update if changed significantly to prevent layout thrashing
        if (Math.abs(availableHeight - currentAiHeight) > 2) {
            groupAi.style.height = availableHeight + "px";
        }
        
        var aiChatHeight = availableHeight - 180; // Reserve space for input area
        var chatContainer = document.getElementById('ai-chat-container');
        chatContainer.style.height = aiChatHeight + "px";
        
        resizeTimeout = null;
    }, 100); // 50ms delay
}

function init() {
    if (window.external) {
        try {
            window.external.OnJsCommand("Init", "");
        } catch (e) {
            console.error("Error calling external: " + e.message);
        }
    }
    // Attach resize event
    
    
    // Initial resize
    
}
// 新增：隐藏重试按钮的辅助函数
function hideRetryButton() {
    var buRetry = document.getElementById('buRetry');
    if (buRetry && buRetry.className.indexOf('hidden') === -1) {
        buRetry.className += ' hidden';
    }
}
// 新增：辅助函数，用于切换“发送”和“停止”按钮的显示状态
function updateButtonVisibility() {
    var buSend = document.getElementById('buSend');
    var buStop = document.getElementById('buStop');
    if (!buSend || !buStop) return;

    if (waitingForReply) {
        // 正在工作：隐藏发送，显示停止
        if (buSend.className.indexOf('hidden') === -1) {
            buSend.className += ' hidden';
        }
        buStop.className = buStop.className.replace(' hidden', '');
    } else {
        // 不在工作：显示发送，隐藏停止
        buSend.className = buSend.className.replace(' hidden', '');
        if (buStop.className.indexOf('hidden') === -1) {
            buStop.className += ' hidden';
        }
    }
}

// C++ calls this when document is loaded
function onReady(dataStr) {
    if(dataStr){
        try {
            var data = JSON.parse(dataStr);
            for (var key in data) {
                if (data.hasOwnProperty(key)) {
                    var element = document.getElementById(key);
                    if (element) {
                        element.value = data[key];
                    }
                }
            }
        } catch (e) {
            console.error("Error processing data from C++: " + e.message);
        }
        window.onresize = resizeLayout;
    }
    resizeLayout();
}
function onReplyContinue(dataStr){
    onReply(dataStr,true)
}
function onReplyStop(dataStr){
    onReply(dataStr,false)
}
function onReply(dataStr,isContinue){
    if (!dataStr) return;

    // 如果正在打字，先停止之前的打字效果
    if (typingInterval) {
        clearInterval(typingInterval);
        typingInterval = null;
    }

    stopCursorBlink(); // 停止思考时的闪烁
    
    var chatContainer = document.getElementById('ai-chat-container');
    var aiResponseText = "昇龙助手: " + dataStr;
    
    var targetDiv;
    if (lastMessage.lastMessageDiv) {
        // 更新已有消息内容
        targetDiv = lastMessage.lastMessageDiv;
    } else {
        // 添加新的聊天记录
        targetDiv = document.createElement('div');
        targetDiv.className = 'chat-message ai';
        chatContainer.appendChild(targetDiv);
    }

    // 清空原来的占位内容和光标
    targetDiv.innerHTML = '';

    // 创建一个 span 用于存放文字，以便光标跟在文字后面
    var textSpan = document.createElement('span');
    targetDiv.appendChild(textSpan);

    // 重新添加一个光标
    var cursor = document.createElement('span');
    cursor.className = 'thinking-cursor';
    targetDiv.appendChild(cursor);
    startCursorBlink(cursor); // 打字时也保持光标闪烁

    var charIndex = 0;
    // 每 30ms 输出一个字符
    typingInterval = setInterval(function() {
        if (charIndex < aiResponseText.length) {
            textSpan.innerText = aiResponseText.substring(0, charIndex + 1);
            charIndex++;
            // 自动滚动到底部，保持最新输出可见
            chatContainer.scrollTop = chatContainer.scrollHeight;
        } else {
            // 输出完毕
            clearInterval(typingInterval);
            typingInterval = null;
            stopCursorBlink();
            cursor.style.visibility = 'hidden'; // 隐藏光标
            // 重置，允许下一条消息
            lastMessage.lastMessageDiv = null;
            if (isContinue){
                if (window.external){
                    window.external.OnJsCommand("ReplyContinue", "");
                }
            }else{
                waitingForReply = false;
                updateButtonVisibility(); // 新增：更新按钮状态
            }
        }
    }, 30); 
}
function onChatError(){
    // 如果正在打字，先停止之前的打字效果
    if (typingInterval) {
        clearInterval(typingInterval);
        typingInterval = null;
    }

    stopCursorBlink(); // 停止思考时的闪烁
    
    var dataStr = "会话失败，请重试！";
    var chatContainer = document.getElementById('ai-chat-container');
    var aiResponseText = "昇龙助手: " + dataStr;
    
    var targetDiv;
    if (lastMessage.lastMessageDiv) {
        // 更新已有消息内容
        targetDiv = lastMessage.lastMessageDiv;
    } else {
        // 添加新的聊天记录
        targetDiv = document.createElement('div');
        targetDiv.className = 'chat-message ai';
        chatContainer.appendChild(targetDiv);
    }

    // 清空原来的占位内容和光标
    targetDiv.innerHTML = aiResponseText;
    lastMessage.lastMessageDiv = null;
    waitingForReply = false;
    updateButtonVisibility(); 

    var buRetry = document.getElementById('buRetry');
    if (buRetry) {
        buRetry.className = buRetry.className.replace(' hidden', '');
    }
}
function toggleGroup(headerElement) {
    var content = headerElement.nextSibling;
    while (content && content.nodeType !== 1) {
        content = content.nextSibling;
    }
    
    if (!content) return;

    // 记录当前组是否处于折叠状态
    var isCollapsed = content.className.indexOf('collapsed') !== -1;

    // 手风琴模式：先折叠所有组
    var allContents = getElementsByClass('group-content', document, 'div');
    for (var i = 0; i < allContents.length; i++) {
        if (allContents[i].className.indexOf('collapsed') === -1) {
            allContents[i].className += ' collapsed';
        }
    }

    // 如果点击的是原本折叠的组，则展开它（原本展开的组保持折叠）
    if (isCollapsed) {
        content.className = content.className.replace(' collapsed', '');
    }

    // Recalculate layout after toggle
    setTimeout(resizeLayout, 100);
}

function callCppMessage() {
    try {
        window.external.ShowMessage("Test");
    } catch (e) {
        alert("Call failed: " + e.message);
    }
}

function updateItem(dataStr) {
    var parts = dataStr.split('|');
    if (parts.length < 4) return;
    var index = parts[0];
    var strVal = parts[1];
    var intVal = parts[2];
    var realVal = parts[3];
    var strEl = document.getElementById('str_' + index);
    var intEl = document.getElementById('int_' + index);
    var realEl = document.getElementById('real_' + index);
    if (strEl) strEl.value = strVal;
    if (intEl) intEl.value = intVal;
    if (realEl) realEl.value = realVal;
}

function onProfileChange(txtObj) {
    var val = txtObj.value;
    var profileValue = {};
    profileValue[txtObj.id] = val;
    var profileValueStr = JSON.stringify(profileValue);
    if (window.external) {
        try {
            window.external.OnJsCommand("SetProfile", profileValueStr);
        } catch (e) {
            alert("Error calling external: " + e.message);
        }
    } else {
        alert("window.external is not available.");
    }
}

function setBackgroundColor(color) {
    document.body.style.backgroundColor = color;
}

function onSelectBlock() {
    if (window.external){
        window.external.OnJsCommand("PreSelectBlock", "");
    }
}
function onSelectLine(){
    if (window.external){
        window.external.OnJsCommand("PreSelectLine", "");
    }
}
function onSelectCircle(){
    if (window.external){
        window.external.OnJsCommand("PreSelectCircle", "");
    }
}
function onScale(){
    if (window.external){
        window.external.OnJsCommand("PreSelectScale", "");
    }
}
function onScaleEntireModel(){
    if (window.external){
        window.external.OnJsCommand("PreSelectScaleWhole", "");
    }
}
function startCursorBlink(cursorEl) {
    stopCursorBlink();
    var isVisible = true;
    blinkInterval = setInterval(function() {
        isVisible = !isVisible;
        cursorEl.style.visibility = isVisible ? 'visible' : 'hidden';
    }, 500); // 500ms toggle
}

function stopCursorBlink() {
    if (blinkInterval) {
        clearInterval(blinkInterval);
        blinkInterval = null;
    }
}
function sendChat() {
    if (lastMessage.lastMessageDiv || waitingForReply){
        alert("请等待上一条消息回复完成！");
        return;
    }
    var input = document.getElementById('ai-chat-input');
    var message = input.value;
    lastUserMessage = message; // 记录最后发送的消息


    // Use the IE7-compatible trim function
    var trimmedMessage = trimString(message);
    
    // Check if the trimmed message is empty
    if (!trimmedMessage) return;

    var chatContainer = document.getElementById('ai-chat-container');
    
    // 1. Add User Message to Array
    chatMessages.push({
        role: 'user',
        content: message
    });
    

    // 2. Update UI for User Message
    var userMsgDiv = document.createElement('div');
    userMsgDiv.className = 'chat-message user';
    userMsgDiv.innerText = message;
    chatContainer.appendChild(userMsgDiv);

    // Clear input immediately
    input.value = '';
    chatContainer.scrollTop = chatContainer.scrollHeight;
    

    var aiResponseText = "昇龙助手:  \"" + lastMessage.text ;
    var aiMsgDiv = document.createElement('div');
    aiMsgDiv.className = 'chat-message ai';
    aiMsgDiv.innerText = aiResponseText;

    // Add blinking cursor to indicate thinking state
    var cursor = document.createElement('span');
    cursor.className = 'thinking-cursor';
    aiMsgDiv.appendChild(cursor);
    startCursorBlink(cursor); // Start JS-based blinking
    

    chatContainer.appendChild(aiMsgDiv);
    
    chatContainer.scrollTop = chatContainer.scrollHeight;

    lastMessage.lastMessageDiv=aiMsgDiv;
    if (window.external){
        window.external.OnJsCommand("Chat", message);
        waitingForReply = true;
        updateButtonVisibility();
    }else{
        waitingForReply = true;
        updateButtonVisibility();
        setTimeout(function(){
            onReply("图像API接口的通用问题汇总，包含接口调试、模型计费与限流、接口高频报错等。\n" +
" 本文涉及的图像模型有：文生图V1和V2、涂鸦作画、图像局部重绘、Cosplay动漫人物生成、人像风格重绘、虚拟模特、鞋靴模特、图像画面扩展、人物实例分割、图像擦除补全、创意海报生成、图像背景生成、图配文。\n" +
"本地调试接口\n" +
"图像API均支持HTTP调用。下面以文生图API为例展示本地调试HTTP接口的流程。\n" +
"需要开通模型服务并获取API Key，再配置API Key到环境变量。\n" +
"在图像API文档中找到curl命令。")
        },2000);
    }

    // 3. Simulate AI Response (In a real app, this would be an async call)
    // setTimeout(function() {
    //     var aiResponseText = "昇龙助手: Received \"" + message + "\". Processing...";
        
    //     // Add AI Message to Array
    //     chatMessages.push({
    //         role: 'ai',
    //         content: aiResponseText
    //     });

    //     // Update UI for AI Message
    //     var aiMsgDiv = document.createElement('div');
    //     aiMsgDiv.className = 'chat-message ai';
    //     aiMsgDiv.innerText = aiResponseText;
    //     chatContainer.appendChild(aiMsgDiv);
        
    //     chatContainer.scrollTop = chatContainer.scrollHeight;
        
    //     // Optional: Log the current state of the message array for debugging/other uses
    //     console.log("Current Chat History:", JSON.stringify(chatMessages));
    // }, 500);
}
function stopChat(){
    hideRetryButton(); // 停止时隐藏重试
    if (!waitingForReply) return;

    // 新增：确认弹出框
    var isConfirmed = confirm("确定要停止当前的回复吗？");
    if (!isConfirmed) return;

    // 通知 C++ 后端停止生成
    if (window.external){
        window.external.OnJsCommand("ChatStop", "");
    }

    // 如果正在打字，停止打字效果
    if (typingInterval) {
        clearInterval(typingInterval);
        typingInterval = null;
    }
    stopCursorBlink();

    // 重置状态
    waitingForReply = false;
    if (lastMessage.lastMessageDiv) {
        // 清除当前消息上的光标
        var cursors = getElementsByClass('thinking-cursor', lastMessage.lastMessageDiv, 'span');
        for(var i=0; i<cursors.length; i++){
            cursors[i].style.visibility = 'hidden';
        }
        lastMessage.lastMessageDiv = null;
    }
    updateButtonVisibility(); // 更新按钮状态
}
function newChat(){
    // 新增：确认弹出框
    var isConfirmed = confirm("确定要开启新会话并清空当前聊天记录吗？");
    if (!isConfirmed) return;

    hideRetryButton(); // 新会话时隐藏重试
    // 1. 停止可能正在进行的打字机和光标闪烁效果
    if (typingInterval) {
        clearInterval(typingInterval);
        typingInterval = null;
    }
    stopCursorBlink();

    // 2. 清空聊天记录数组
    chatMessages.length = 0; 
    
    // 3. 清空聊天界面并恢复初始欢迎语
    var chatContainer = document.getElementById('ai-chat-container');
    if (chatContainer) {
        chatContainer.innerHTML = '<div class="chat-message ai">昇龙助手: 您好! 今天我能帮助您做点什么吗?</div><hr style="border: 0; border-top: 1px dashed #cccccc; margin: 10px 0; color: #cccccc;">';
    }

    // 4. 重置状态标志
    lastMessage.lastMessageDiv = null;
    waitingForReply = false;
    updateButtonVisibility(); 

    // 5. 通知 C++ 后端开启新会话 (修复了原代码中 message 变量未定义的问题)
    if (window.external){
        window.external.OnJsCommand("ChatNew", "");
    }
}
// 新增：重试发送上一条消息
function retryChat() {
    if (!lastUserMessage || waitingForReply) return;
    
    hideRetryButton(); // 开始重试，隐藏按钮

    var chatContainer = document.getElementById('ai-chat-container');
    
    // 显示思考状态
    var aiResponseText = "昇龙助手:  \"" + lastMessage.text ;
    var aiMsgDiv = document.createElement('div');
    aiMsgDiv.className = 'chat-message ai';
    aiMsgDiv.innerText = aiResponseText;

    var cursor = document.createElement('span');
    cursor.className = 'thinking-cursor';
    aiMsgDiv.appendChild(cursor);
    startCursorBlink(cursor); 
    
    chatContainer.appendChild(aiMsgDiv);
    chatContainer.scrollTop = chatContainer.scrollHeight;

    lastMessage.lastMessageDiv=aiMsgDiv;
    
    if (window.external){
        window.external.OnJsCommand("ChatRetry", lastUserMessage);
        waitingForReply = true;
        updateButtonVisibility();
    }else{
        waitingForReply = true;
        updateButtonVisibility();
        setTimeout(function(){
            onReply("重试成功！");
        },2000);
    }
}
function checkEnter(event) {
    if (event.keyCode === 13 && event.ctrlKey) {
        sendChat();
    }
}
// ==== 功能菜单 ====
function onMenuCmd(cmd) {
    if (window.external) {
        try {
            window.external.OnJsCommand(cmd, "");
        } catch (e) {
            alert("调用 " + cmd + " 失败: " + e.message);
        }
    }
}