#!/usr/bin/env python3
# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""RN2K/1 bounded raw Classic-CAN TCP bridge. Python 3.9+ standard library only."""
import argparse, asyncio, hashlib, hmac, ipaddress, json, os, secrets, socket, struct, time, subprocess, contextlib

MAX_LINE = 1024
FRAME = struct.Struct('=IB3x8s')

class Bridge:
    def __init__(self, interface, key=b'', virtual=False, allow=()):
        self.interface, self.key, self.virtual = interface, key, virtual
        self.allow = [ipaddress.ip_network(n) for n in allow]
        self.peers, self.sequence, self.bus, self.fault = set(), 0, None, False
        self.pacer=asyncio.Lock();self.next_tx=0

    async def start(self):
        if self.virtual: return
        self.bus = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        # No receive-own-messages: network TX is explicitly forwarded exactly once.
        self.bus.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_RECV_OWN_MSGS, 0)
        self.bus.setsockopt(socket.SOL_CAN_RAW, getattr(socket,"CAN_RAW_ERR_FILTER",2), struct.pack('=I', 0x1fffffff))
        self.bus.bind((self.interface,)); self.bus.setblocking(False)
        self.check_health()
        asyncio.create_task(self.read_bus())
        asyncio.create_task(self.monitor())

    def check_health(self):
        link=json.loads(subprocess.check_output(['ip','-details','-statistics','-json','link','show','dev',self.interface],timeout=2))[0]
        info=link.get('linkinfo',{});data=info.get('info_data',{})
        if not ('UP' in link['flags']): raise ValueError("Validation failed")
        if info.get('info_kind')!='vcan':
            if not (data.get('state')=='ERROR-ACTIVE' and data.get('bittiming',{}).get('bitrate')==250000): raise ValueError("Validation failed")
            if not (not any(info.get('info_xstats',{}).values())): raise ValueError("Validation failed")
            for side in ('rx','tx'):
                if not (not link['stats64'][side]['errors'] and not link['stats64'][side]['dropped']): raise ValueError("Validation failed")

    async def monitor(self):
        try:
            while True:
                await asyncio.sleep(.5);await asyncio.to_thread(self.check_health)
        except Exception:
            self.fault=True
            for p in tuple(self.peers):self.emit(p,{'kind':'error','message':'CAN health fault; TX revoked'});p['writer'].close()

    def emit(self, peer, obj):
        if peer['writer'].transport.get_write_buffer_size() > 65536:
            peer['writer'].close(); return
        peer['writer'].write((json.dumps(obj, separators=(',', ':'))+'\n').encode())

    def broadcast(self, frame, origin):
        self.sequence += 1
        obj=dict(frame, kind='frame', seq=self.sequence, origin=origin, timestamp_ns=time.time_ns())
        for peer in tuple(self.peers):
            if peer['id'] != origin: self.emit(peer, obj)

    async def read_bus(self):
        loop=asyncio.get_running_loop()
        try:
            while True:
                raw=await loop.sock_recv(self.bus, 16)
                if len(raw)!=16: raise ValueError('Invalid CAN frame')
                canid,dlc,data=FRAME.unpack(raw)
                if canid & 0x20000000: raise ValueError('CAN error frame: TX revoked')
                self.broadcast({'id':canid,'dlc':dlc,'data':data[:dlc].hex()}, 'bus')
        except Exception:
            self.fault=True
            for p in tuple(self.peers): self.emit(p, {'kind':'error','message':'CAN fault; reconnect only after inspection'}); p['writer'].close()

    async def transmit(self, p):
        try:
            while True:
                frame,queued=await p['queue'].get()
                async with self.pacer:
                    if self.bus:
                        while time.monotonic()<self.next_tx:
                            await asyncio.sleep(max(.001,self.next_tx-time.monotonic()))
                    now=time.monotonic()
                    if p['closed'] or p['writer'].is_closing() or self.fault: return
                    if now-queued>2 or now>=min(p['until'],p['hard']) or p['mode']=='rx': raise ValueError('TX lease/queue expired')
                    if self.bus:
                        if self.bus.send(FRAME.pack(frame['id'],frame['dlc'],bytes.fromhex(frame['data']).ljust(8,b'\0')))!=16: raise OSError('CAN write rejected')
                        self.next_tx=now+.005
                    self.broadcast(frame,p['id'])
        except asyncio.CancelledError: pass
        except Exception as error:
            if isinstance(error,OSError):
                self.fault=True
                for other in tuple(self.peers):self.emit(other,{"kind":"error","message":"CAN write fault; all TX revoked"});other["writer"].close()
            print(json.dumps({'event':'tx_stopped','reason':type(error).__name__}),flush=True)
            self.emit(p,{'kind':'error','message':'TX failed or expired; permission revoked'})
            p['writer'].close()

    async def client(self, reader, writer):
        # Hashable identity wrapper; no owner route/credential contents in logs.
        class Peer(dict):
            __hash__=object.__hash__
        p=Peer(writer=writer,id=secrets.token_hex(8),mode='rx',until=0,hard=0,frames=0,nonce=secrets.token_hex(32),rate=[],queue=asyncio.Queue(maxsize=128),closed=False,authorized=False)
        worker=asyncio.create_task(self.transmit(p))
        try:
            address=ipaddress.ip_address(writer.get_extra_info('peername')[0])
            if self.allow and not any(address in n for n in self.allow): return
            if len(self.peers)>=16 or self.fault: return
            self.peers.add(p)
            self.emit(p, {'kind':'hello','version':1,'interface':self.interface,'client':p['id'],'nonce':p['nonce']})
            while True:
                line=await asyncio.wait_for(reader.readline(), 5)
                if not line: break
                if len(line)>MAX_LINE: raise ValueError('Line bound')
                m=json.loads(line)
                if not isinstance(m,dict): raise ValueError('Envelope')
                k=m.get('kind'); now=time.monotonic()
                if k=='auth':
                    if p['authorized']: raise ValueError('Reconnect to reauthorize')
                    mode=m.get('mode')
                    if not (mode in ('rx','tx','service') and m.get('interface')==self.interface): raise ValueError("Validation failed")
                    expected=hmac.new(self.key, (p['nonce']+'|'+p['id']+'|'+mode+'|'+self.interface).encode(),hashlib.sha256).hexdigest()
                    if mode!='rx' and (not self.key or not hmac.compare_digest(str(m.get('proof','')),expected)): raise ValueError('Authorization denied')
                    p.update(authorized=True,mode=mode,until=now+3,hard=now+60 if mode=='service' else float('inf'),frames=0,nonce=secrets.token_hex(32))
                    self.emit(p,{'kind':'ready','mode':mode,'nonce':p['nonce']})
                elif k=='ping':
                    if p['mode']!='rx' and now>=min(p['until'],p['hard']): raise ValueError('Lease expired')
                    p['until']=now+3; self.emit(p,{'kind':'pong'})
                elif k=='frame':
                    if self.fault or p['mode']=='rx' or now>=min(p['until'],p['hard']): raise ValueError('TX not authorized')
                    canid=m.get('id'); dlc=m.get('dlc'); data=bytes.fromhex(m.get('data',''))
                    if not (type(canid) is int and type(dlc) is int and 0<=canid<=0xffffffff and 0<=dlc<=8 and len(data)==dlc): raise ValueError("Validation failed")
                    if not (not canid&0x20000000 and (canid&0x80000000 or canid&0x1fffffff<=0x7ff)): raise ValueError("Validation failed")
                    # NMEA service lease coarse raw-ID allowlist; application also
                    # applies the precise Group Function selector policy.
                    pg=(canid>>8)&0x3ffff; pg=pg&0x3ff00 if (pg&0xff00)<0xf000 else pg
                    if p['mode']=='service' and (pg not in (59392,59904,60928,126208,126464,126993,126996,126998) or p['frames']>=512): raise ValueError('Service bound')
                    p['rate']=[t for t in p['rate'] if now-t<1]
                    if len(p['rate'])>=500: raise ValueError('TX rate bound')
                    p['rate'].append(now);p['frames']+=1
                    p['queue'].put_nowait(({'id':canid,'dlc':dlc,'data':data.hex()},now))
                else: raise ValueError('Unknown command')
                await writer.drain()
        except (Exception, asyncio.CancelledError) as error:
            print(json.dumps({"event":"session_closed","mode":p["mode"],"reason":str(error)[:80]}),flush=True)
            if not writer.is_closing(): self.emit(p,{'kind':'error','message':'Session rejected or expired; TX revoked'})
        finally:
            p['closed']=True;worker.cancel()
            with contextlib.suppress(asyncio.CancelledError): await worker
            self.peers.discard(p);writer.close()
            try: await writer.wait_closed()
            except Exception: pass

async def run(args):
    key=open(args.key_file,'rb').read().strip() if args.key_file else b''
    if os.name=="posix" and args.key_file and (os.stat(args.key_file).st_mode&0o077): raise SystemExit('Key file must have mode0600')
    b=Bridge(args.interface,key,args.virtual,args.allow_client)
    await b.start()
    async with await asyncio.start_server(b.client,args.bind,args.port,limit=MAX_LINE+1) as server:
        print('RN2K/1 port='+str(server.sockets[0].getsockname()[1])+'; RX default; '+('virtual bus' if args.virtual else 'SocketCAN')+'; explicit authenticated TX only',flush=True)
        await server.serve_forever()

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--interface',default='can0');p.add_argument('--bind',default='127.0.0.1');p.add_argument('--port',type=int,default=29501)
    p.add_argument('--key-file',help='0600 shared TX key; omit for RX-only server');p.add_argument('--allow-client',action='append',default=[]);p.add_argument('--virtual',action='store_true',help='In-process virtual bus when vcan is unavailable')
    try: asyncio.run(run(p.parse_args()))
    except KeyboardInterrupt: pass
