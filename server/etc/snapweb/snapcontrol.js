"use strict";
class Host {
    constructor(json) {
        this.fromJson(json);
    }
    fromJson(json) {
        this.arch = json.arch;
        this.ip = json.ip;
        this.mac = json.mac;
        this.name = json.name;
        this.os = json.os;
    }
    arch = "";
    ip = "";
    mac = "";
    name = "";
    os = "";
}
class Client {
    constructor(json) {
        this.fromJson(json);
    }
    fromJson(json) {
        this.id = json.id;
        this.host = new Host(json.host);
        let jsnapclient = json.snapclient;
        this.snapclient = { name: jsnapclient.name, protocolVersion: jsnapclient.protocolVersion, version: jsnapclient.version };
        let jconfig = json.config;
        this.config = { instance: jconfig.instance, latency: jconfig.latency, name: jconfig.name, volume: { muted: jconfig.volume.muted, percent: jconfig.volume.percent } };
        this.lastSeen = { sec: json.lastSeen.sec, usec: json.lastSeen.usec };
        this.connected = Boolean(json.connected);
    }
    id = "";
    host;
    snapclient;
    config;
    lastSeen;
    connected = false;
}
class Group {
    constructor(json) {
        this.fromJson(json);
    }
    fromJson(json) {
        this.name = json.name;
        this.id = json.id;
        this.stream_id = json.stream_id;
        this.muted = Boolean(json.muted);
        for (let client of json.clients)
            this.clients.push(new Client(client));
    }
    name = "";
    id = "";
    stream_id = "";
    muted = false;
    clients = [];
    getClient(id) {
        for (let client of this.clients) {
            if (client.id == id)
                return client;
        }
        return null;
    }
}
class Metadata {
    constructor(json) {
        this.fromJson(json);
    }
    fromJson(json) {
        this.title = json.title;
        this.artist = json.artist;
        this.album = json.album;
        this.artUrl = json.artUrl;
        this.duration = json.duration;
    }
    title;
    artist;
    album;
    artUrl;
    duration;
}
class Properties {
    constructor(json) {
        this.fromJson(json);
    }
    fromJson(json) {
        this.loopStatus = json.loopStatus;
        this.shuffle = json.shuffle;
        this.volume = json.volume;
        this.rate = json.rate;
        this.playbackStatus = json.playbackStatus;
        this.position = json.position;
        this.minimumRate = json.minimumRate;
        this.maximumRate = json.maximumRate;
        this.canGoNext = Boolean(json.canGoNext);
        this.canGoPrevious = Boolean(json.canGoPrevious);
        this.canPlay = Boolean(json.canPlay);
        this.canPause = Boolean(json.canPause);
        this.canSeek = Boolean(json.canSeek);
        this.canControl = Boolean(json.canControl);
        if (json.metadata != undefined) {
            this.metadata = new Metadata(json.metadata);
        }
        else {
            this.metadata = new Metadata({});
        }
    }
    loopStatus;
    shuffle;
    volume;
    rate;
    playbackStatus;
    position;
    minimumRate;
    maximumRate;
    canGoNext = false;
    canGoPrevious = false;
    canPlay = false;
    canPause = false;
    canSeek = false;
    canControl = false;
    metadata;
}
class Stream {
    constructor(json) {
        this.fromJson(json);
    }
    fromJson(json) {
        this.id = json.id;
        this.status = json.status;
        if (json.properties != undefined) {
            this.properties = new Properties(json.properties);
        }
        else {
            this.properties = new Properties({});
        }
        let juri = json.uri;
        this.uri = { raw: juri.raw, scheme: juri.scheme, host: juri.host, path: juri.path, fragment: juri.fragment, query: juri.query };
    }
    id = "";
    status = "";
    uri;
    properties;
}
class Server {
    constructor(json) {
        if (json)
            this.fromJson(json);
    }
    fromJson(json) {
        this.groups = [];
        for (let jgroup of json.groups)
            this.groups.push(new Group(jgroup));
        let jsnapserver = json.server.snapserver;
        this.server = { host: new Host(json.server.host), snapserver: { controlProtocolVersion: jsnapserver.controlProtocolVersion, name: jsnapserver.name, protocolVersion: jsnapserver.protocolVersion, version: jsnapserver.version } };
        this.streams = [];
        for (let jstream of json.streams) {
            this.streams.push(new Stream(jstream));
        }
    }
    groups = [];
    server;
    streams = [];
    getClient(id) {
        for (let group of this.groups) {
            let client = group.getClient(id);
            if (client)
                return client;
        }
        return null;
    }
    getGroup(id) {
        for (let group of this.groups) {
            if (group.id == id)
                return group;
        }
        return null;
    }
    getStream(id) {
        for (let stream of this.streams) {
            if (stream.id == id)
                return stream;
        }
        return null;
    }
}
class SnapControl {
    constructor(baseUrl) {
        this.server = new Server();
        this.baseUrl = baseUrl;
        this.msg_id = 0;
        this.status_req_id = -1;
        this.connect();
    }
    connect() {
        this.connection = new WebSocket(this.baseUrl + '/jsonrpc');
        this.connection.onmessage = (msg) => this.onMessage(msg.data);
        this.connection.onopen = () => { this.status_req_id = this.sendRequest('Server.GetStatus'); };
        this.connection.onerror = (ev) => { console.error('error:', ev); };
        this.connection.onclose = () => {
            console.info('connection lost, reconnecting in 1s');
            setTimeout(() => this.connect(), 1000);
        };
    }
    onNotification(notification) {
        let stream;
        switch (notification.method) {
            case 'Client.OnVolumeChanged':
                let client = this.getClient(notification.params.id);
                client.config.volume = notification.params.volume;
                updateGroupVolume(this.getGroupFromClient(client.id));
                return true;
            case 'Client.OnLatencyChanged':
                this.getClient(notification.params.id).config.latency = notification.params.latency;
                return false;
            case 'Client.OnNameChanged':
                this.getClient(notification.params.id).config.name = notification.params.name;
                return true;
            case 'Client.OnConnect':
            case 'Client.OnDisconnect':
                this.getClient(notification.params.client.id).fromJson(notification.params.client);
                return true;
            case 'Group.OnMute':
                this.getGroup(notification.params.id).muted = Boolean(notification.params.mute);
                return true;
            case 'Group.OnStreamChanged':
                this.getGroup(notification.params.id).stream_id = notification.params.stream_id;
                this.updateProperties(notification.params.stream_id);
                return true;
            case 'Stream.OnUpdate':
                stream = this.getStream(notification.params.id);
                stream.fromJson(notification.params.stream);
                this.updateProperties(stream.id);
                return true;
            case 'Server.OnUpdate':
                this.server.fromJson(notification.params.server);
                this.updateProperties(this.getMyStreamId());
                return true;
            case 'Stream.OnProperties':
                stream = this.getStream(notification.params.id);
                stream.properties.fromJson(notification.params.properties);
                if (this.getMyStreamId() == stream.id)
                    this.updateProperties(stream.id);
                return false;
            default:
                return false;
        }
    }
    updateProperties(stream_id) {
        if (!('mediaSession' in navigator)) {
            console.log('updateProperties: mediaSession not supported');
            return;
        }
        if (stream_id != this.getMyStreamId()) {
            console.log('updateProperties: not my stream id: ' + stream_id + ', mine: ' + this.getMyStreamId());
            return;
        }
        let props;
        let metadata;
        try {
            props = this.getStreamFromClient(SnapStream.getClientId()).properties;
            metadata = this.getStreamFromClient(SnapStream.getClientId()).properties.metadata;
        }
        catch (e) {
            console.log('updateProperties failed: ' + e);
            return;
        }
        // https://developers.google.com/web/updates/2017/02/media-session
        // https://github.com/googlechrome/samples/tree/gh-pages/media-session
        // https://googlechrome.github.io/samples/media-session/audio.html
        // https://developer.mozilla.org/en-US/docs/Web/API/MediaSession/setActionHandler#seekto
        console.log('updateProperties: ', props);
        let play_state = "none";
        if (props.playbackStatus != undefined) {
            if (props.playbackStatus == "playing") {
                audio.play();
                play_state = "playing";
            }
            else if (props.playbackStatus == "paused") {
                audio.pause();
                play_state = "paused";
            }
            else if (props.playbackStatus == "stopped") {
                audio.pause();
                play_state = "none";
            }
        }
        let mediaSession = navigator.mediaSession;
        mediaSession.playbackState = play_state;
        console.log('updateProperties playbackState: ', navigator.mediaSession.playbackState);
        // if (props.canGoNext == undefined || !props.canGoNext!)
        mediaSession.setActionHandler('play', () => {
            props.canPlay ?
                this.sendRequest('Stream.Control', { id: stream_id, command: 'play' }) : null;
        });
        mediaSession.setActionHandler('pause', () => {
            props.canPause ?
                this.sendRequest('Stream.Control', { id: stream_id, command: 'pause' }) : null;
        });
        mediaSession.setActionHandler('previoustrack', () => {
            props.canGoPrevious ?
                this.sendRequest('Stream.Control', { id: stream_id, command: 'previous' }) : null;
        });
        mediaSession.setActionHandler('nexttrack', () => {
            props.canGoNext ?
                this.sendRequest('Stream.Control', { id: stream_id, command: 'next' }) : null;
        });
        try {
            mediaSession.setActionHandler('stop', () => {
                props.canControl ?
                    this.sendRequest('Stream.Control', { id: stream_id, command: 'stop' }) : null;
            });
        }
        catch (error) {
            console.log('Warning! The "stop" media session action is not supported.');
        }
        let defaultSkipTime = 10; // Time to skip in seconds by default
        mediaSession.setActionHandler('seekbackward', (event) => {
            let offset = (event.seekOffset || defaultSkipTime) * -1;
            if (props.position != undefined)
                Math.max(props.position + offset, 0);
            props.canSeek ?
                this.sendRequest('Stream.Control', { id: stream_id, command: 'seek', params: { 'offset': offset } }) : null;
        });
        mediaSession.setActionHandler('seekforward', (event) => {
            let offset = event.seekOffset || defaultSkipTime;
            if ((metadata.duration != undefined) && (props.position != undefined))
                Math.min(props.position + offset, metadata.duration);
            props.canSeek ?
                this.sendRequest('Stream.Control', { id: stream_id, command: 'seek', params: { 'offset': offset } }) : null;
        });
        try {
            mediaSession.setActionHandler('seekto', (event) => {
                let position = event.seekTime || 0;
                if (metadata.duration != undefined)
                    Math.min(position, metadata.duration);
                props.canSeek ?
                    this.sendRequest('Stream.Control', { id: stream_id, command: 'setPosition', params: { 'position': position } }) : null;
            });
        }
        catch (error) {
            console.log('Warning! The "seekto" media session action is not supported.');
        }
        if ((metadata.duration != undefined) && (props.position != undefined) && (props.position <= metadata.duration)) {
            if ('setPositionState' in mediaSession) {
                console.log('Updating position state: ' + props.position + '/' + metadata.duration);
                mediaSession.setPositionState({
                    duration: metadata.duration,
                    playbackRate: 1.0,
                    position: props.position
                });
            }
        }
        else {
            mediaSession.setPositionState({
                duration: 0,
                playbackRate: 1.0,
                position: 0
            });
        }
        console.log('updateMetadata: ', metadata);
        // https://github.com/Microsoft/TypeScript/issues/19473
        let title = metadata.title || "Unknown Title";
        let artist = (metadata.artist != undefined) ? metadata.artist[0] : "Unknown Artist";
        let album = metadata.album || "";
        let artwork = metadata.artUrl || 'snapcast-512.png';
        console.log('Metadata title: ' + title + ', artist: ' + artist + ', album: ' + album + ", artwork: " + artwork);
        navigator.mediaSession.metadata = new MediaMetadata({
            title: title,
            artist: artist,
            album: album,
            artwork: [
                // { src: artwork, sizes: '250x250', type: 'image/jpeg' },
                // 'https://dummyimage.com/96x96', sizes: '96x96', type: 'image/png' },
                { src: artwork, sizes: '128x128', type: 'image/png' },
                { src: artwork, sizes: '192x192', type: 'image/png' },
                { src: artwork, sizes: '256x256', type: 'image/png' },
                { src: artwork, sizes: '384x384', type: 'image/png' },
                { src: artwork, sizes: '512x512', type: 'image/png' },
            ]
        });
        // mediaSession.setActionHandler('seekbackward', function () { });
        // mediaSession.setActionHandler('seekforward', function () { });
    }
    getClient(client_id) {
        let client = this.server.getClient(client_id);
        if (client == null) {
            throw new Error(`client ${client_id} was null`);
        }
        return client;
    }
    getGroup(group_id) {
        let group = this.server.getGroup(group_id);
        if (group == null) {
            throw new Error(`group ${group_id} was null`);
        }
        return group;
    }
    getGroupVolume(group, online) {
        if (group.clients.length == 0)
            return 0;
        let group_vol = 0;
        let client_count = 0;
        for (let client of group.clients) {
            if (online && !client.connected)
                continue;
            group_vol += client.config.volume.percent;
            ++client_count;
        }
        if (client_count == 0)
            return 0;
        return group_vol / client_count;
    }
    getGroupFromClient(client_id) {
        for (let group of this.server.groups)
            for (let client of group.clients)
                if (client.id == client_id)
                    return group;
        throw new Error(`group for client ${client_id} was null`);
    }
    getStreamFromClient(client_id) {
        let group = this.getGroupFromClient(client_id);
        return this.getStream(group.stream_id);
    }
    getMyStreamId() {
        try {
            let group = this.getGroupFromClient(SnapStream.getClientId());
            return this.getStream(group.stream_id).id;
        }
        catch (e) {
            return "";
        }
    }
    getStream(stream_id) {
        let stream = this.server.getStream(stream_id);
        if (stream == null) {
            throw new Error(`stream ${stream_id} was null`);
        }
        return stream;
    }
    setVolume(client_id, percent, mute) {
        percent = Math.max(0, Math.min(100, percent));
        let client = this.getClient(client_id);
        client.config.volume.percent = percent;
        if (mute != undefined)
            client.config.volume.muted = mute;
        this.sendRequest('Client.SetVolume', { id: client_id, volume: { muted: client.config.volume.muted, percent: client.config.volume.percent } });
    }
    setClientName(client_id, name) {
        let client = this.getClient(client_id);
        let current_name = (client.config.name != "") ? client.config.name : client.host.name;
        if (name != current_name) {
            this.sendRequest('Client.SetName', { id: client_id, name: name });
            client.config.name = name;
        }
    }
    setClientLatency(client_id, latency) {
        let client = this.getClient(client_id);
        let current_latency = client.config.latency;
        if (latency != current_latency) {
            this.sendRequest('Client.SetLatency', { id: client_id, latency: latency });
            client.config.latency = latency;
        }
    }
    deleteClient(client_id) {
        this.sendRequest('Server.DeleteClient', { id: client_id });
        this.server.groups.forEach((g, gi) => {
            g.clients.forEach((c, ci) => {
                if (c.id == client_id) {
                    this.server.groups[gi].clients.splice(ci, 1);
                }
            });
        });
        this.server.groups.forEach((g, gi) => {
            if (g.clients.length == 0) {
                this.server.groups.splice(gi, 1);
            }
        });
        show();
    }
    setStream(group_id, stream_id) {
        this.getGroup(group_id).stream_id = stream_id;
        this.updateProperties(stream_id);
        this.sendRequest('Group.SetStream', { id: group_id, stream_id: stream_id });
    }
    setClients(group_id, clients) {
        this.status_req_id = this.sendRequest('Group.SetClients', { id: group_id, clients: clients });
    }
    muteGroup(group_id, mute) {
        this.getGroup(group_id).muted = mute;
        this.sendRequest('Group.SetMute', { id: group_id, mute: mute });
    }
    sendRequest(method, params) {
        let msg = {
            id: ++this.msg_id,
            jsonrpc: '2.0',
            method: method
        };
        if (params)
            msg.params = params;
        let msgJson = JSON.stringify(msg);
        console.log("Sending: " + msgJson);
        this.connection.send(msgJson);
        return this.msg_id;
    }
    onMessage(msg) {
        let json_msg = JSON.parse(msg);
        let is_response = (json_msg.id != undefined);
        console.log("Received " + (is_response ? "response" : "notification") + ", json: " + JSON.stringify(json_msg));
        if (is_response) {
            if (json_msg.id == this.status_req_id) {
                this.server = new Server(json_msg.result.server);
                this.updateProperties(this.getMyStreamId());
                show();
            }
        }
        else {
            let refresh = false;
            if (Array.isArray(json_msg)) {
                for (let notification of json_msg) {
                    refresh = this.onNotification(notification) || refresh;
                }
            }
            else {
                refresh = this.onNotification(json_msg);
            }
            // TODO: don't update everything, but only the changed, 
            // e.g. update the values for the volume sliders
            if (refresh)
                show();
        }
    }
    baseUrl;
    connection;
    server;
    msg_id;
    status_req_id;
}
let snapcontrol;
let snapstream = null;
let hide_offline = true;
let autoplay_done = false;
let audio = document.createElement('audio');
function autoplayRequested() {
    return document.location.hash.match(/autoplay/) !== null;
}
function show() {
    // Render the page
    const versionElem = document.getElementsByTagName("meta").namedItem("version");
    console.log("Snapweb version " + (versionElem ? versionElem.content : "null"));
    let play_img;
    if (snapstream) {
        play_img = 'stop.png';
    }
    else {
        play_img = 'play.png';
    }
    let content = "";
    content += "<div class='navbar'>Snapcast";
    let serverVersion = snapcontrol.server.server.snapserver.version.split('.');
    if ((serverVersion.length >= 2) && (+serverVersion[1] >= 21)) {
        content += "    <img src='" + play_img + "' class='play-button' id='play-button'></a>";
        // Stream became ready and was not playing. If autoplay is requested, start playing.
        if (!snapstream && !autoplay_done && autoplayRequested()) {
            autoplay_done = true;
            play();
        }
    }
    content += "</div>";
    content += "<div class='content'>";
    let server = snapcontrol.server;
    for (let group of server.groups) {
        if (hide_offline) {
            let groupActive = false;
            for (let client of group.clients) {
                if (client.connected) {
                    groupActive = true;
                    break;
                }
            }
            if (!groupActive)
                continue;
        }
        // Set mute variables
        let classgroup;
        let muted;
        let mute_img;
        if (group.muted == true) {
            classgroup = 'group muted';
            muted = true;
            mute_img = 'mute_icon.png';
        }
        else {
            classgroup = 'group';
            muted = false;
            mute_img = 'speaker_icon.png';
        }
        // Start group div
        content += "<div id='g_" + group.id + "' class='" + classgroup + "'>";
        // Create stream selection dropdown
        let streamselect = "<select id='stream_" + group.id + "' onchange='setStream(\"" + group.id + "\")' class='stream'>";
        for (let i_stream = 0; i_stream < server.streams.length; i_stream++) {
            let streamselected = "";
            if (group.stream_id == server.streams[i_stream].id) {
                streamselected = 'selected';
            }
            streamselect += "<option value='" + server.streams[i_stream].id + "' " + streamselected + ">" + server.streams[i_stream].id + ": " + server.streams[i_stream].status + "</option>";
        }
        streamselect += "</select>";
        // Group mute and refresh button
        content += "<div class='groupheader'>";
        content += streamselect;
        // let cover_img: string = server.getStream(group.stream_id)!.properties.metadata.artUrl || "snapcast-512.png";
        // content += "<img src='" + cover_img + "' class='cover-img' id='cover_" + group.id + "'>";
        let clientCount = 0;
        for (let client of group.clients)
            if (!hide_offline || client.connected)
                clientCount++;
        if (clientCount > 1) {
            let volume = snapcontrol.getGroupVolume(group, hide_offline);
            // content += "<div class='client'>";
            content += "<a href=\"javascript:setMuteGroup('" + group.id + "'," + !muted + ");\"><img src='" + mute_img + "' class='mute-button'></a>";
            content += "<div class='slidergroupdiv'>";
            content += "    <input type='range' draggable='false' min=0 max=100 step=1 id='vol_" + group.id + "' oninput='javascript:setGroupVolume(\"" + group.id + "\")' value=" + volume + " class='slider'>";
            // content += "    <input type='range' min=0 max=100 step=1 id='vol_" + group.id + "' oninput='javascript:setVolume(\"" + client.id + "\"," + client.config.volume.muted + ")' value=" + client.config.volume.percent + " class='" + sliderclass + "'>";
            content += "</div>";
            // content += "</div>";
        }
        // transparent placeholder edit icon
        content += "<div class='edit-group-icon'>&#9998</div>";
        content += "</div>";
        content += "<hr class='groupheader-separator'>";
        // Create clients in group
        for (let client of group.clients) {
            if (!client.connected && hide_offline)
                continue;
            // Set name and connection state vars, start client div
            let name;
            let clas = 'client';
            if (client.config.name != "") {
                name = client.config.name;
            }
            else {
                name = client.host.name;
            }
            if (client.connected == false) {
                clas = 'client disconnected';
            }
            content += "<div id='c_" + client.id + "' class='" + clas + "'>";
            // Client mute status vars
            let muted;
            let mute_img;
            let sliderclass;
            if (client.config.volume.muted == true) {
                muted = true;
                sliderclass = 'slider muted';
                mute_img = 'mute_icon.png';
            }
            else {
                sliderclass = 'slider';
                muted = false;
                mute_img = 'speaker_icon.png';
            }
            // Populate client div
            content += "<a href=\"javascript:setVolume('" + client.id + "'," + !muted + ");\"><img src='" + mute_img + "' class='mute-button'></a>";
            content += "    <div class='sliderdiv'>";
            content += "        <input type='range' min=0 max=100 step=1 id='vol_" + client.id + "' oninput='javascript:setVolume(\"" + client.id + "\"," + client.config.volume.muted + ")' value=" + client.config.volume.percent + " class='" + sliderclass + "'>";
            content += "    </div>";
            content += "    <span class='edit-icons'>";
            content += "        <a href=\"javascript:openClientSettings('" + client.id + "');\" class='edit-icon'>&#9998</a>";
            if (client.connected == false) {
                content += "      <a href=\"javascript:deleteClient('" + client.id + "');\" class='delete-icon'>&#128465</a>";
                content += "   </span>";
            }
            else {
                content += "</span>";
            }
            content += "    <div class='name'>" + name + "</div>";
            content += "</div>";
        }
        content += "</div>";
    }
    content += "</div>"; // content
    content += "<div id='client-settings' class='client-settings'>";
    content += "    <div class='client-setting-content'>";
    content += "        <form action='javascript:closeClientSettings()'>";
    content += "        <label for='client-name'>Name</label>";
    content += "        <input type='text' class='client-input' id='client-name' name='client-name' placeholder='Client name..'>";
    content += "        <label for='client-latency'>Latency</label>";
    content += "        <input type='number' class='client-input' min='-10000' max='10000' id='client-latency' name='client-latency' placeholder='Latency in ms..'>";
    content += "        <label for='client-group'>Group</label>";
    content += "        <select id='client-group' class='client-input' name='client-group'>";
    content += "        </select>";
    content += "        <input type='submit' value='Submit'>";
    content += "        </form>";
    content += "    </div>";
    content += "</div>";
    // Pad then update page
    content = content + "<br><br>";
    document.getElementById('show').innerHTML = content;
    let playElem = document.getElementById('play-button');
    playElem.onclick = () => {
        play();
    };
    for (let group of snapcontrol.server.groups) {
        if (group.clients.length > 1) {
            let slider = document.getElementById("vol_" + group.id);
            if (slider == null)
                continue;
            slider.addEventListener('pointerdown', function () {
                groupVolumeEnter(group.id);
            });
            slider.addEventListener('touchstart', function () {
                groupVolumeEnter(group.id);
            });
        }
    }
}
function updateGroupVolume(group) {
    let group_vol = snapcontrol.getGroupVolume(group, hide_offline);
    let slider = document.getElementById("vol_" + group.id);
    if (slider == null)
        return;
    console.log("updateGroupVolume group: " + group.id + ", volume: " + group_vol + ", slider: " + (slider != null));
    slider.value = String(group_vol);
}
let client_volumes;
let group_volume;
function setGroupVolume(group_id) {
    let group = snapcontrol.getGroup(group_id);
    let percent = document.getElementById('vol_' + group.id).valueAsNumber;
    console.log("setGroupVolume id: " + group.id + ", volume: " + percent);
    // show()
    let delta = percent - group_volume;
    let ratio;
    if (delta < 0)
        ratio = (group_volume - percent) / group_volume;
    else
        ratio = (percent - group_volume) / (100 - group_volume);
    for (let i = 0; i < group.clients.length; ++i) {
        let new_volume = client_volumes[i];
        if (delta < 0)
            new_volume -= ratio * client_volumes[i];
        else
            new_volume += ratio * (100 - client_volumes[i]);
        let client_id = group.clients[i].id;
        // TODO: use batch request to update all client volumes at once
        snapcontrol.setVolume(client_id, new_volume);
        let slider = document.getElementById('vol_' + client_id);
        if (slider)
            slider.value = String(new_volume);
    }
}
function groupVolumeEnter(group_id) {
    let group = snapcontrol.getGroup(group_id);
    let percent = document.getElementById('vol_' + group.id).valueAsNumber;
    console.log("groupVolumeEnter id: " + group.id + ", volume: " + percent);
    group_volume = percent;
    client_volumes = [];
    for (let i = 0; i < group.clients.length; ++i) {
        client_volumes.push(group.clients[i].config.volume.percent);
    }
    // show()
}
function setVolume(id, mute) {
    console.log("setVolume id: " + id + ", mute: " + mute);
    let percent = document.getElementById('vol_' + id).valueAsNumber;
    let client = snapcontrol.getClient(id);
    let needs_update = (mute != client.config.volume.muted);
    snapcontrol.setVolume(id, percent, mute);
    let group = snapcontrol.getGroupFromClient(id);
    updateGroupVolume(group);
    if (needs_update)
        show();
}
function play() {
    if (snapstream) {
        snapstream.stop();
        snapstream = null;
        audio.pause();
        audio.src = '';
        document.body.removeChild(audio);
    }
    else {
        snapstream = new SnapStream(config.baseUrl);
        // User interacted with the page. Let's play audio...
        document.body.appendChild(audio);
        audio.src = "10-seconds-of-silence.mp3";
        audio.loop = true;
        audio.play().then(() => {
            snapcontrol.updateProperties(snapcontrol.getMyStreamId());
        });
    }
}
function setMuteGroup(id, mute) {
    snapcontrol.muteGroup(id, mute);
    show();
}
function setStream(id) {
    snapcontrol.setStream(id, document.getElementById('stream_' + id).value);
    show();
}
function setGroup(client_id, group_id) {
    console.log("setGroup id: " + client_id + ", group: " + group_id);
    let server = snapcontrol.server;
    // Get client group id
    let current_group = snapcontrol.getGroupFromClient(client_id);
    // Get
    //   List of target group's clients
    // OR
    //   List of current group's other clients
    let send_clients = [];
    for (let i_group = 0; i_group < server.groups.length; i_group++) {
        if (server.groups[i_group].id == group_id || (group_id == "new" && server.groups[i_group].id == current_group.id)) {
            for (let i_client = 0; i_client < server.groups[i_group].clients.length; i_client++) {
                if (group_id == "new" && server.groups[i_group].clients[i_client].id == client_id) { }
                else {
                    send_clients[send_clients.length] = server.groups[i_group].clients[i_client].id;
                }
            }
        }
    }
    if (group_id == "new")
        group_id = current_group.id;
    else
        send_clients[send_clients.length] = client_id;
    snapcontrol.setClients(group_id, send_clients);
}
function setName(id) {
    // Get current name and lacency
    let client = snapcontrol.getClient(id);
    let current_name = (client.config.name != "") ? client.config.name : client.host.name;
    let current_latency = client.config.latency;
    let new_name = window.prompt("New Name", current_name);
    let new_latency = Number(window.prompt("New Latency", String(current_latency)));
    if (new_name != null)
        snapcontrol.setClientName(id, new_name);
    if (new_latency != null)
        snapcontrol.setClientLatency(id, new_latency);
    show();
}
function openClientSettings(id) {
    let modal = document.getElementById("client-settings");
    let client = snapcontrol.getClient(id);
    let current_name = (client.config.name != "") ? client.config.name : client.host.name;
    let name = document.getElementById("client-name");
    name.name = id;
    name.value = current_name;
    let latency = document.getElementById("client-latency");
    latency.valueAsNumber = client.config.latency;
    let group = snapcontrol.getGroupFromClient(id);
    let group_input = document.getElementById("client-group");
    while (group_input.length > 0)
        group_input.remove(0);
    let group_num = 0;
    for (let ogroup of snapcontrol.server.groups) {
        let option = document.createElement('option');
        option.value = ogroup.id;
        option.text = "Group " + (group_num + 1) + " (" + ogroup.clients.length + " Clients)";
        group_input.add(option);
        if (ogroup == group) {
            console.log("Selected: " + group_num);
            group_input.selectedIndex = group_num;
        }
        ++group_num;
    }
    let option = document.createElement('option');
    option.value = option.text = "new";
    group_input.add(option);
    modal.style.display = "block";
}
function closeClientSettings() {
    let name = document.getElementById("client-name");
    let id = name.name;
    console.log("onclose " + id + ", value: " + name.value);
    snapcontrol.setClientName(id, name.value);
    let latency = document.getElementById("client-latency");
    snapcontrol.setClientLatency(id, latency.valueAsNumber);
    let group_input = document.getElementById("client-group");
    let option = group_input.options[group_input.selectedIndex];
    setGroup(id, option.value);
    let modal = document.getElementById("client-settings");
    modal.style.display = "none";
    show();
}
function deleteClient(id) {
    if (confirm('Are you sure?')) {
        snapcontrol.deleteClient(id);
    }
}
window.onload = function () {
    snapcontrol = new SnapControl(config.baseUrl);
};
// When the user clicks anywhere outside of the modal, close it
window.onclick = function (event) {
    let modal = document.getElementById("client-settings");
    if (event.target == modal) {
        modal.style.display = "none";
    }
};
//# sourceMappingURL=snapcontrol.js.map