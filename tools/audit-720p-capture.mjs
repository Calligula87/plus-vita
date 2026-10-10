// Bounded, sample-specific SPS experiment. Never use this to patch live streams.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { spawnSync } from 'node:child_process';
const [input, toolDir, outputPrefix] = process.argv.slice(2);
if (!input || !toolDir || !outputPrefix) throw Error('Usage: node audit-720p-capture.mjs INPUT TOOL_DIR OUTPUT_PREFIX');
function run(tool, args) {
  const p = spawnSync(path.join(toolDir, `${tool}.exe`), args, { encoding: 'utf8', maxBuffer: 32 * 1024 * 1024 });
  if (p.error || p.status !== 0) throw Error(p.error || p.stderr);
  return p;
}
const source = fs.readFileSync(input);
if (source.length > 8 * 1024 * 1024) throw Error('Capture too large');
const trace = run('ffmpeg', ['-hide_banner', '-i', input, '-c:v', 'copy', '-bsf:v', 'trace_headers', '-f', 'null', 'NUL']).stderr;
const blocks = [];
let current;
for (const line of trace.split(/\r?\n/)) {
  const kind = line.match(/\] (Sequence Parameter Set|Picture Parameter Set|Slice Header)$/);
  if (kind) { current = { kind: kind[1], fields: {} }; blocks.push(current); continue; }
  const field = line.match(/\]\s+(\d+)\s+(\w+)\s+([01]+)\s+=\s+(-?\d+)$/);
  if (field && current) current.fields[field[2]] = { bit: +field[1], bits: field[3], value: +field[4] };
}
function requireField(block, name, value) {
  if (block.fields[name]?.value !== value) throw Error(`Unsupported ${block.kind}: ${name} != ${value}`);
}
const sps = blocks.filter(b => b.kind === 'Sequence Parameter Set');
const pps = blocks.filter(b => b.kind === 'Picture Parameter Set');
const slices = blocks.filter(b => b.kind === 'Slice Header');
for (const b of sps) for (const [k,v] of Object.entries({profile_idc:77,level_idc:40,seq_parameter_set_id:0,log2_max_frame_num_minus4:3,pic_order_cnt_type:2,max_num_ref_frames:9,gaps_in_frame_num_allowed_flag:0,pic_width_in_mbs_minus1:79,pic_height_in_map_units_minus1:44,frame_mbs_only_flag:1,max_num_reorder_frames:0,max_dec_frame_buffering:9})) requireField(b,k,v);
for (const b of pps) for (const [k,v] of Object.entries({pic_parameter_set_id:0,seq_parameter_set_id:0,num_slice_groups_minus1:0,num_ref_idx_l0_default_active_minus1:0,weighted_pred_flag:0,weighted_bipred_idc:0,redundant_pic_cnt_present_flag:0})) requireField(b,k,v);
if (!sps.length || !pps.length || !slices.length) throw Error('Missing headers');
let pictures = 0, lastFrame = -1, currentFrame = -1, idrSlices = 0, pSlices = 0;
for (const b of slices) {
  requireField(b,'pic_parameter_set_id',0);
  const frame = b.fields.frame_num?.value;
  if (b.fields.first_mb_in_slice?.value === 0) {
    if (pictures === 0) { requireField(b,'nal_unit_type',5); if (frame !== 0) throw Error('First frame not IDR zero'); }
    else if (frame !== (lastFrame + 1) % 128) throw Error('Frame-number gap');
    currentFrame = lastFrame = frame; pictures++;
  } else if (frame !== currentFrame) throw Error('Slice without picture start');
  if (b.fields.nal_unit_type?.value === 5) {
    if (pictures !== 1) throw Error('Additional IDR requires extended audit');
    requireField(b,'slice_type',7); requireField(b,'long_term_reference_flag',0); idrSlices++;
  } else {
    requireField(b,'nal_unit_type',1); requireField(b,'nal_ref_idc',2); requireField(b,'slice_type',5);
    requireField(b,'num_ref_idx_active_override_flag',0); requireField(b,'ref_pic_list_modification_flag_l0',0);
    requireField(b,'adaptive_ref_pic_marking_mode_flag',0); pSlices++;
  }
}
if (pictures !== 300) throw Error(`Expected 300 pictures, got ${pictures}`);
// Find SPS and remove emulation-prevention bytes for FFmpeg's RBSP bit offsets.
const nals = [];
for (let i=0; i+3<source.length; i++) {
  if (source[i]===0 && source[i+1]===0 && source[i+2]===1) nals.push({start:i>0 && source[i-1]===0 ? i-1:i, header:i+3});
}
const targets = nals.filter(n => (source[n.header]&31)===7);
if (targets.length !== 1) throw Error('Expected one SPS NAL');
const n = targets[0], next = nals[nals.indexOf(n)+1];
const end = next?.start ?? source.length;
const rbsp = [];
let zeros=0;
for (let i=n.header;i<end;i++) {
  const v=source[i];
  if (zeros>=2 && v===3) { zeros=0; continue; }
  rbsp.push(v); zeros=v===0 ? zeros+1:0;
}
let bits=rbsp.map(v=>v.toString(2).padStart(8,'0')).join('');
const patchFields=['max_num_ref_frames','max_dec_frame_buffering'].map(k=>sps[0].fields[k]).sort((a,b)=>b.bit-a.bit);
// Five is UE 00110. Only these two declarations change; level remains 4.0.
for (const f of patchFields) {
  if(bits.slice(f.bit,f.bit+f.bits.length)!==f.bits) throw Error('SPS trace/bytes mismatch');
  bits=bits.slice(0,f.bit)+'00110'+bits.slice(f.bit+f.bits.length);
}
// Rebuild trailing zero alignment after the original rbsp_stop_one_bit.
bits=bits.replace(/0+$/,''); bits=bits.padEnd(Math.ceil(bits.length/8)*8,'0');
const encoded=[]; zeros=0;
for(let i=0;i<bits.length;i+=8) {
  const v=parseInt(bits.slice(i,i+8),2);
  if(zeros>=2 && v<=3) { encoded.push(3); zeros=0; }
  encoded.push(v); zeros=v===0?zeros+1:0;
}
const patched=Buffer.concat([source.subarray(0,n.header),Buffer.from(encoded),source.subarray(end)]);
const patchedPath=`${outputPrefix}-refs5.h264`;
fs.writeFileSync(patchedPath,patched);
function frameHashes(file) {
  return run('ffmpeg',['-hide_banner','-v','error','-err_detect','explode','-i',file,'-fps_mode','passthrough','-f','framemd5','-']).stdout.split(/\r?\n/).filter(l=>l && !l.startsWith('#')).map(l=>l.split(',').at(-1).trim());
}
const before=frameHashes(input), after=frameHashes(patchedPath);
if(before.length!==300 || JSON.stringify(before)!==JSON.stringify(after)) throw Error('Decoded frame mismatch; do not deploy');
const packets=JSON.parse(run('ffprobe',['-v','error','-show_packets','-show_entries','packet=pos,size','-of','json',patchedPath]).stdout).packets;
if(packets.length!==300) throw Error('AU count mismatch');
const header=Buffer.alloc(28); header.write('PVREPL01');
[1280,720,5,packets.length,patched.length].forEach((v,i)=>header.writeUInt32LE(v,8+i*4));
const records=[header]; let pos=0;
for(const p of packets) {
  const size=+p.size; if(+p.pos!==pos || !size || size>1024*1024) throw Error('Packet bounds/gap');
  const length=Buffer.alloc(4); length.writeUInt32LE(size); records.push(length,patched.subarray(pos,pos+size)); pos+=size;
}
if(pos!==patched.length) throw Error('Unaccounted bytes');
const replay=Buffer.concat(records), replayPath=`${outputPrefix}.bin`;
fs.writeFileSync(replayPath,replay);
const report={sample:input,source_sha256:crypto.createHash('sha256').update(source).digest('hex'),pictures,idrSlices,pSlices,active_list0_entries:1,reference_list_modifications:0,adaptive_marking:0,long_term_references:0,b_frames:0,frame_num_gaps:0,original_declared_refs:9,patched_declared_refs:5,identical_decoded_frames:after.length,replayPath,replay_sha256:crypto.createHash('sha256').update(replay).digest('hex'),scope:'This captured sample only; not proof for future live streams'};
fs.writeFileSync(`${outputPrefix}-audit.json`,JSON.stringify(report,null,2)+'\n'); console.log(JSON.stringify(report,null,2));
