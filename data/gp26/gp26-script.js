const timeout = 5000; // 5 seconds
const keepAliveInterval = 10000; // 10 seconds

// Global variable to track mute state
let isMuted = false;


// Javascript to handle section collapsing
var coll = document.getElementsByClassName("collapsible");
var i;

for (i = 0; i < coll.length; i++) {
  coll[i].addEventListener("click", function() {
    this.classList.toggle("active");
    var content = this.nextElementSibling;
    if (content.style.maxHeight){
      content.style.maxHeight = null;
    } else {
      content.style.maxHeight = content.scrollHeight + "px";
    } 
  });
}


// Initial page load constructors
window.onload = function(event) {
    console.log('onload');
    initWebSocket();
    setInterval(keepAlive, keepAliveInterval); // Check WebSocket connection every 10 seconds
}

function initWebSocket() {
    websocket = new WebSocket('ws://' + window.location.hostname + '/ws');
    websocket.onopen = function(event) { 
        console.log('Connected to WebSocket'); 
        updateConnectionStatus(true);
        getTeamNames(); // Call the function to get team names from the websocket
        getCountdownTimer(); // get the current value of the countdown slider
        loadCustomAnnouncements(); // Call the function to load announcements
    };
    websocket.onclose = function(event) { 
        console.log('Disconnected from WebSocket'); 
        updateConnectionStatus(false);
        setTimeout(initWebSocket, timeout); // Attempt to reconnect after 5 seconds
    };
    websocket.onmessage = function(event) {
        try {
            handleWebSocketMessage(JSON.parse(event.data));
        } catch (e) {
            console.error('Invalid JSON 1.3.0:', event.data);
        }
    };
    websocket.onerror = function(event) { 
        console.error('WebSocket error:', event); 
        updateConnectionStatus(false);
    }; // Add error handling
}

function keepAlive() {
    if (!websocket || websocket.readyState === WebSocket.CLOSED) {
        console.log('WebSocket is closed, attempting to reconnect...');
        initWebSocket();
    }
}

// Handle incoming WebSocket messages
function handleWebSocketMessage(message) {
    if (message.type === 'update') {
        // console.log('handle JS Websocket update: ', message);
        updateTeamsUI(message.data);
    }  else if (message.type === 'pilotSwap') {
       // console.log('handle JS Websocket pilotSwap: ', message);
        const teamId = 'team' + message.team;
        pilotSwap(teamId);
    } else if (message.type === 'updateCustomMessages') {
        // Update custom messages
        document.getElementById('customMessageBefore').value = message.customMessageBefore;
        document.getElementById('customMessageAfter').value = message.customMessageAfter;
    } else if (message.type === 'updateTeamNames') {
        // Load team names
        //console.log('Loading team names from message:', message.teamNames); // Add debugging information
        loadTeamNames(message.teamNames);
    } else if  (message.type === 'timerUpdate') {
        updateCountdownDisplay(message.timerValue);
    } else {
        console.log('Unknown WebSocket message type:', message.type);
    }
}


// takes select element and teamId as arguments
// sends a websocket with json data to update the team name.
function updateTeamName(selectElement, teamId) {
    console.log('updateTeamName: ', teamId, " -> ", selectElement.value);
    const selectedTeamName = selectElement.value;
    
    websocket.send(JSON.stringify({ type: 'update', teamId: teamId, teamName: selectedTeamName })); // Ensure correct data format
}

// takes single team update with name, id, countdown and buttonId.
// sends or returns nothing. 
function updateTeamBox(teamName, teamId, countdown, buttonId) {
    // console.log('updateTeamBox: ', teamId, " -> ", teamName, countdown, buttonId);
    document.querySelector(`#${teamId} .team-name`).textContent = teamName;

    const teamBox = document.getElementById(teamId);
    const button = document.getElementById(buttonId);
    if (countdown > 0) {
        // console.log('Disable the button');
        button.disabled = true;
        teamBox.style.backgroundColor = 'yellow';
    } else {
        // console.log('Enable the button');
        button.disabled = false;
        teamBox.style.backgroundColor = '';
    }

}

// takes teamId as arguments for a single lane update.
// sends a websocket with json data to notify other clients.
// function pilotSwap(teamId) {
function pilotSwap(teamId) {
    const teamBox = document.getElementById(teamId);
    const teamName = teamBox.querySelector('.team-name').textContent;
    console.log('pilotSwap: ', teamId, " -> ", teamName);

    const customMessageInputBefore = document.getElementById('customMessageBefore');
    const customMessageInputAfter = document.getElementById('customMessageAfter');
    const announcement = customMessageInputBefore.value + " " + teamName + " " + customMessageInputAfter.value;
    voiceAnnounce(announcement);
}

function pilotSwapStart (teamId, buttonId) {
    const teamID = teamId.substring(4); // get the team number off the end of the teamId
    websocket.send(JSON.stringify({ type: 'pilotSwap', teamId: teamID, buttonId: buttonId })); // Tell the websocket which lane to start
}

// hack to force IOS devices to play audio on an event
let hasEnabledVoice = false;

document.addEventListener('click', () => {
  if (hasEnabledVoice) {
    return;
  }
  const lecture = new SpeechSynthesisUtterance('hello');
  lecture.volume = 0;
  speechSynthesis.speak(lecture);
  hasEnabledVoice = true;
});

function voiceAnnounce(text) {
    const announcement = new SpeechSynthesisUtterance(text);
    // Set volume based on mute state
    announcement.volume = isMuted ? 0 : 1;
    window.speechSynthesis.speak(announcement);
}

function toggleMute() {
    const button = document.getElementById('muteButton');
    
    if (isMuted) {
        // Unmute - set volume to 100%
        button.textContent = '🔊';
        button.title = 'Mute announcements';
        isMuted = false;
    } else {
        // Mute - set volume to 0%
        button.textContent = '🔇';
        button.title = 'Unmute announcements';
        isMuted = true;
    }
    
    // Note: Browser volume control is not directly accessible via JavaScript for security reasons
    // The mute state is tracked for UI purposes and can be used to control speech synthesis volume
}


function saveCustomAnnouncements() {
    const customMessageInputBefore = document.getElementById('customMessageBefore');
    const customMessageInputAfter = document.getElementById('customMessageAfter');

    // Send custom messages to WebSocket
    websocket.send(JSON.stringify({
        type: 'updateCustomMessages',
        customMessageBefore: customMessageInputBefore.value,
        customMessageAfter: customMessageInputAfter.value
    }));
}

function loadCustomAnnouncements() {
    // Send request to WebSocket to get custom messages
    websocket.send(JSON.stringify({ type: 'getCustomMessages' }));
}

function updateTeamsUI(UIdata) {
    for (var i = 0; i < UIdata.length; i++) {
        const teamName = UIdata[i].teamName;
        const countdown = UIdata[i].countdown;
        const teamId = 'team' + (i + 1);
        const buttonId = 'pilotSwapButton' + (i + 1);

        updateTeamBox(teamName, teamId, countdown, buttonId);
    }
}

function updateConnectionStatus(isConnected) {
    const statusIndicator = document.getElementById('connectionStatusIndicator');
    const statusText = document.getElementById('connectionStatusText');
    if (isConnected) {
        statusIndicator.classList.remove('disconnected');
        statusIndicator.classList.add('connected');
        statusText.textContent = 'Connected';
    } else {
        statusIndicator.classList.remove('connected');
        statusIndicator.classList.add('disconnected');
        statusText.textContent = 'Disconnected';
    }
}

// Team Names Table Functions
function getTeamNames() {
    websocket.send(JSON.stringify({ type: 'getTeamNames' }));
}

// Team names table and functions
function buildTeamNamesTable(teamNames) {
    const tableBody = document.getElementById('teamNamesTableBody');
    tableBody.innerHTML = ''; // Clear existing rows

    teamNames.forEach(name => {
        const row = document.createElement('tr');
        row.setAttribute('draggable', 'true');
        row.setAttribute('ondragstart', 'startDrag(event)');
        row.setAttribute('ondragover', 'dragOver(event)');

        const dragHandleCell = document.createElement('td');
        dragHandleCell.className = 'drag-handle';
        dragHandleCell.textContent = '☰';

        const nameCell = document.createElement('td');
        nameCell.setAttribute('contenteditable', 'true');
        nameCell.textContent = name;

        const removeButtonCell = document.createElement('td');
        const removeButton = document.createElement('button');
        removeButton.textContent = 'Remove';
        removeButton.setAttribute('onclick', 'removeTeamName(this)');
        removeButtonCell.appendChild(removeButton);

        row.appendChild(dragHandleCell);
        row.appendChild(nameCell);
        row.appendChild(removeButtonCell);

        tableBody.appendChild(row);
    });
}

// Function to update the dropdown lists with the team names
function loadTeamNames(TeamNamesList) {
    const savedTeamNames = Array.isArray(TeamNamesList) ? TeamNamesList : JSON.parse(TeamNamesList) || [];
    // console.log('Loading Saved Team Names:', savedTeamNames);
    const teamNames = savedTeamNames.length > 0 ? savedTeamNames : Array.from(document.querySelectorAll('#teamNamesTable tbody tr td:nth-child(2)')).map(td => td.textContent);
    const dropdowns = ['team1Dropdown', 'team2Dropdown', 'team3Dropdown', 'team4Dropdown'];
    dropdowns.forEach(dropdownId => {
        const dropdown = document.getElementById(dropdownId);
        dropdown.innerHTML = '<option value="">-- Select --</option>';
        teamNames.forEach(name => {
            const option = document.createElement('option');
            option.value = name;
            option.textContent = name;
            dropdown.appendChild(option);
        });
    });
    buildTeamNamesTable(teamNames);
    // console.log('Team Names Loaded:', teamNames);
}

function saveTeamNames() {
    const teamNames = Array.from(document.querySelectorAll('#teamNamesTable tbody tr td:nth-child(2)')).map(td => td.textContent);
    // Send the team names to the websocket to be saved
    websocket.send(JSON.stringify({ type: 'updateTeamNames', teamNames: teamNames }));
    getTeamNames(); // Reload the team names after saving
}

function addTeamName() {
    const table = document.getElementById('teamNamesTable').getElementsByTagName('tbody')[0];
    const newRow = table.insertRow();
    newRow.draggable = true;
    newRow.setAttribute('ondragOver' , 'dragOver(event)');
    newRow.setAttribute('ondragstart', 'startDrag(event)');
    const newCell1 = newRow.insertCell(0);
    const newCell2 = newRow.insertCell(1);
    const newCell3 = newRow.insertCell(2);
    newCell1.className = "drag-handle";
    newCell1.textContent = "☰";
    newCell2.contentEditable = "true";
    newCell2.textContent = "New Team";
    newCell3.innerHTML = '<button onclick="removeTeamName(this)">Remove</button>';
}

function removeTeamName(button) {
    const row = button.parentNode.parentNode;
    row.parentNode.removeChild(row);
}

var pickedRow;
var pickedRowText;
var pickedRowIndex;


function startDrag(event) {
    try {
        pickedRow = event.target;
        pickedRowIndex = pickedRow.closest("tr").rowIndex;
        pickedRowText = pickedRow.closest("tr").getElementsByTagName("td")[1].textContent;
        event.dataTransfer.setData("text/plain", pickedRowText);
    } catch (error) {
        console.error('Drag error:', error);
    }
}

function dragOver(event) {
    event.preventDefault();

    const targetRow = event.target.closest("tr");

    if (isBefore(pickedRow, targetRow)) {
        pickedRow.parentNode.insertBefore(pickedRow, targetRow);
    } else {
        pickedRow.parentNode.insertBefore(pickedRow, targetRow.nextSibling);
    }
 
}

function isBefore(pickedRow, targetRow) {
    if (targetRow.parentNode === pickedRow.parentNode)
      for (var cur = pickedRow.previousSibling; cur && cur.nodeType !== 9; cur = cur.previousSibling)
        if (cur === targetRow)
          return true;
    return false;
  }

function updateCountdownTimer(timer) {
    // console.log("Countdown timer new value: " + timer);
    websocket.send(JSON.stringify({ type: 'updateCountdownTimer', timerValue: timer }));
}

function adjustCountdown(amount) {
    const slider = document.getElementById('countdownSlider');
    slider.value = Math.max(0, Math.min(300, parseInt(slider.value, 10) + amount));
    updateCountdownDisplay(slider.value);
}

function updateCountdownDisplay(value) {
    document.getElementById('countdownDisplay').textContent = value;
}

function getCountdownTimer() {
    // console.log("Asking for the countdown value.");
    websocket.send(JSON.stringify({ type: 'getCountdownTimer' }));
}

function updateBGColour(section, colour) {
    document.getElementById(section).style.backgroundColor = colour;
}

function populateVoiceList() {
    if (typeof speechSynthesis === "undefined") {
        return;
    }

    const voices = speechSynthesis.getVoices();

    for (let i = 0; i < voices.length; i++) {
        const option = document.createElement("option");
        option.textContent = `${voices[i].name} (${voices[i].lang})`;

        if (voices[i].default) {
            option.textContent += " — DEFAULT";
        }

        option.setAttribute("data-lang", voices[i].lang);
        option.setAttribute("data-name", voices[i].name);
        document.getElementById("speechPicker").appendChild(option);
    }
}

populateVoiceList();
if (
    typeof speechSynthesis !== "undefined" &&
    speechSynthesis.onvoiceschanged !== undefined
) {
    speechSynthesis.onvoiceschanged = populateVoiceList;
}