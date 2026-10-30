# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
import asyncio, importlib.util, json, hmac, hashlib, unittest, time
from pathlib import Path
spec=importlib.util.spec_from_file_location('bridge',Path(__file__).parents[1]/'tools/network-can/raynmea_bridge.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Protocol(unittest.IsolatedAsyncioTestCase):
 async def asyncSetUp(self):
  self.b=m.Bridge('lab',b'test-key',True);self.s=await asyncio.start_server(self.b.client,'127.0.0.1',0,limit=1025);self.port=self.s.sockets[0].getsockname()[1];self.writers=[]
 async def asyncTearDown(self):
  for w in self.writers:w.close();await w.wait_closed()
  self.s.close();await self.s.wait_closed();await asyncio.sleep(.01)
 async def peer(self,mode='rx',key=b'test-key'):
  r,w=await asyncio.open_connection('127.0.0.1',self.port);self.writers.append(w);hello=json.loads(await r.readline());proof=hmac.new(key,(hello['nonce']+'|'+hello['client']+'|'+mode+'|lab').encode(),hashlib.sha256).hexdigest();w.write((json.dumps(dict(kind='auth',interface='lab',mode=mode,proof=proof))+'\n').encode());await w.drain();reply=json.loads(await r.readline());return r,w,hello,reply
 async def send(self,w,**j):w.write((json.dumps(j)+'\n').encode());await w.drain()
 async def test_rx_cannot_send(self):
  r,w,_,_=await self.peer();await self.send(w,kind='frame',id=0x89f8012c,dlc=1,data='01');self.assertEqual(json.loads(await r.readline())['kind'],'error')
 async def test_authenticated_multiclient_no_origin_echo(self):
  a,aw,ah,_=await self.peer('tx');b,bw,_,_=await self.peer('tx');c,cw,_,_=await self.peer()
  for _ in range(2):await self.send(aw,kind='frame',id=0x89f8012c,dlc=8,data='0011223344556677')
  seen=[json.loads(await b.readline()) for _ in range(2)];other=[json.loads(await c.readline()) for _ in range(2)]
  self.assertEqual([x['seq'] for x in seen],[1,2]);self.assertEqual(seen,other);self.assertEqual(seen[0]['origin'],ah['client']);self.assertEqual(seen[0]['id'],0x89f8012c)
  with self.assertRaises(asyncio.TimeoutError):await asyncio.wait_for(a.readline(),.05)
 async def test_invalid_auth_and_expired_lease(self):
  _,_,_,reply=await self.peer('tx',b'bad');self.assertEqual(reply['kind'],'error')
  r,w,_,_=await self.peer('tx');next(p for p in self.b.peers if p['mode']=='tx')['until']=0;await self.send(w,kind='frame',id=0x89f8012c,dlc=0,data='');self.assertEqual(json.loads(await r.readline())['kind'],'error')
 async def test_service_rejects_instruments_and_writes_after_disconnect(self):
  r,w,_,_=await self.peer('service');await self.send(w,kind='frame',id=0x89f8012c,dlc=0,data='');self.assertEqual(json.loads(await r.readline())['kind'],'error');self.assertEqual(self.b.sequence,0)
 async def test_pacing_and_disconnect_cancels_pending(self):
  class Bus:
   def __init__(self):self.sent=[]
   def send(self,b):self.sent.append(time.monotonic());return 16
  bus=Bus();self.b.bus=bus;self.b.next_tx=time.monotonic()+.15
  r,w,_,_=await self.peer('tx');await self.send(w,kind='frame',id=0x89f8012c,dlc=0,data='');w.close();await w.wait_closed();await asyncio.sleep(.2);self.assertEqual(bus.sent,[])
  r,w,_,_=await self.peer('tx')
  for _ in range(4):await self.send(w,kind='frame',id=0x89f8012c,dlc=0,data='')
  await asyncio.sleep(.1);self.assertEqual(len(bus.sent),4);self.assertTrue(all(b-a>=.004 for a,b in zip(bus.sent,bus.sent[1:])))
 async def test_service_allows_required_iso_acknowledgment(self):
  r,w,_,_=await self.peer('service');rx,_,_,_=await self.peer();await self.send(w,kind='frame',id=0x98e8012c,dlc=8,data='0000000000000000');frame=json.loads(await rx.readline());self.assertEqual(frame['id'],0x98e8012c)
 async def test_service_hard_expiry_during_operator_delay_and_fresh_authorization(self):
  r,w,_,_=await self.peer('service');peer=next(p for p in self.b.peers if p['mode']=='service');hard=peer['hard']
  await self.send(w,kind='ping');self.assertEqual(json.loads(await r.readline())['kind'],'pong');self.assertEqual(peer['hard'],hard)
  peer['hard']=time.monotonic()-1
  await self.send(w,kind='ping');self.assertEqual(json.loads(await r.readline())['kind'],'error')
  self.assertEqual(self.b.sequence,0)
  # A new explicitly authenticated connection gets a fresh bounded lease.
  nr,nw,_,ready=await self.peer('service');self.assertEqual(ready['kind'],'ready')
  await self.send(nw,kind='ping');self.assertEqual(json.loads(await nr.readline())['kind'],'pong')
 async def test_service_frame_budget_and_no_in_connection_renewal(self):
  r,w,_,_=await self.peer('service');rx,_,_,_=await self.peer();peer=next(p for p in self.b.peers if p['mode']=='service');peer['frames']=511
  await self.send(w,kind='frame',id=0x98ea012c,dlc=3,data='00ee00');self.assertEqual(json.loads(await rx.readline())['kind'],'frame')
  await self.send(w,kind='frame',id=0x98ea012c,dlc=3,data='00ee00');self.assertEqual(json.loads(await r.readline())['kind'],'error');self.assertEqual(self.b.sequence,1)
  r,w,_,_=await self.peer('service');await self.send(w,kind='auth',mode='service',interface='lab',proof='')
  self.assertEqual(json.loads(await r.readline())['kind'],'error')
 async def test_service_queue_pressure_revokes_without_unbounded_buffer(self):
  class Bus:
   def send(self,data):return 16
  self.b.bus=Bus();self.b.next_tx=time.monotonic()+1
  r,w,_,_=await self.peer('service')
  frames=(json.dumps(dict(kind='frame',id=0x98ea012c,dlc=3,data='00ee00'))+'\n')*132
  w.write(frames.encode());await w.drain()
  self.assertEqual(json.loads(await asyncio.wait_for(r.readline(),2))['kind'],'error');self.assertEqual(self.b.sequence,0)
 async def test_malformed_and_bounds(self):
  for frame in [dict(id=0x89f8012c,dlc=9,data='00'*9),dict(id=0x29f8012c,dlc=0,data=''),dict(id=0x89f8012c,dlc=1,data='zz')]:
   r,w,_,_=await self.peer('tx');await self.send(w,kind='frame',**frame);self.assertEqual(json.loads(await r.readline())['kind'],'error')
if __name__=='__main__':unittest.main()
