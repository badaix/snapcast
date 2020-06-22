#!/usr/bin/python3
import asyncio
import snapcast.control

loop = asyncio.get_event_loop()
server = loop.run_until_complete(snapcast.control.create_server(loop, 'localhost'))

def on_stream_update(stream):
    if stream.status == 'playing':
	for group in server.groups:
	    asyncio.run_coroutine_threadsafe(group.set_stream(stream.name), loop)
	    print("Group %s is now playing stream %s" % (group.friendly_name,stream.name))

print("Started listening for stream events ....")
try:
    for stream in server.streams:
	stream.set_callback(on_stream_update)
    loop.run_forever()
except KeyboardInterrupt:
    pass
finally:
    loop.close()
