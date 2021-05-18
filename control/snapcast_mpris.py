#!/usr/bin/env python3

import websocket
import logging
import threading
import time
import json


class SnapcastRpcListener:
    def on_snapserver_stream_pause(self):
        pass

    def on_snapserver_stream_start(self, stream_name, stream_group):
        pass

    def on_snapserver_volume_change(self, volume_level):
        pass

    def on_snapserver_mute(self):
        pass

    def on_snapserver_unmute(self):
        pass


class SnapcastRpcWebsocketWrapper:
    def __init__(self, server_address: str, server_control_port, client_id, listener: SnapcastRpcListener):
        self.websocket = websocket.WebSocketApp("ws://" + server_address + ":" + str(server_control_port) + "/jsonrpc",
                                                on_message=self.on_ws_message,
                                                on_error=self.on_ws_error,
                                                on_open=self.on_ws_open,
                                                on_close=self.on_ws_close)

        self.websocket_thread = threading.Thread(
            target=self.websocket_loop, args=())
        self.websocket_thread.name = "SnapcastRpcWebsocketWrapper"
        self.websocket_thread.start()

    def websocket_loop(self):
        logging.info("Started SnapcastRpcWebsocketWrapper loop")
        self.websocket.run_forever()
        logging.info("Ending SnapcastRpcWebsocketWrapper loop")

    def on_ws_message(self, ws, message):
        logging.debug("Snapcast RPC websocket message received")
        logging.debug(message)
        jmsg = json.loads(message)
        if jmsg["method"] == "Stream.OnMetadata":
            logging.info(f'Stream meta changed for "{jmsg["params"]["id"]}"')
            logging.info(f'Meta: "{jmsg["params"]["meta"]}"')

        # json_data = json.loads(message)
        # handlers = self.get_event_handlers_mapping()
        # event = json_data["method"]
        # handlers[event](json_data["params"])

    def on_ws_error(self, ws, error):
        logging.error("Snapcast RPC websocket error")
        logging.error(error)

    def on_ws_open(self, ws):
        logging.info("Snapcast RPC websocket opened")
        # self.websocket.send(json.dumps(
        #     {"id": 1, "jsonrpc": "2.0", "method": "Server.GetStatus"}))

    def on_ws_close(self, ws):
        logging.info("Snapcast RPC websocket closed")
        self.healthy = False

    def stop(self):
        self.websocket.keep_running = False
        logging.info("Waiting for websocket thread to exit")
        # self.websocket_thread.join()


logging.basicConfig(level=logging.INFO)
wrapper = SnapcastRpcWebsocketWrapper("127.0.0.1", 1780, "id", any)
# while True:
#     logging.info("Loop")
#     time.sleep(1)
