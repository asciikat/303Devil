"""Validate fresh WAVs rendered by Windows REAPER, not by our offline test tool."""
import array, json, math, sys, wave
from pathlib import Path
out=Path(sys.argv[1]);report={}
for phase in ('initial','recall'):
    marker=out/f'passed-{phase}.txt'
    assert marker.is_file(), f'Missing REAPER success marker: {phase}'
    with wave.open(str(out/f'punk-fish-{phase}.wav'),'rb') as w:
        rate=w.getframerate(); channels=w.getnchannels(); frames=w.getnframes()
        assert rate==44100 and channels==2 and w.getsampwidth()==2, (rate,channels,w.getsampwidth())
        assert 7.99 <= frames/rate <= 8.1, frames/rate
        samples=array.array('h',w.readframes(frames))
        if sys.byteorder!='little':samples.byteswap()
    peak=max(abs(x) for x in samples)/32768
    rms=math.sqrt(sum((x/32768)**2 for x in samples)/len(samples))
    clip=sum(abs(x)>=32760 for x in samples)/len(samples)
    assert peak>0.0001 and rms>0.00001, f'{phase}: render is silent'
    assert clip<0.001, f'{phase}: excessive full-scale samples ({clip:.3%})'
    report[phase]={'sample_rate':rate,'channels':channels,'seconds':frames/rate,
                   'peak_dbfs':20*math.log10(peak),'rms_dbfs':20*math.log10(rms),
                   'full_scale_fraction':clip,'host':marker.read_text().splitlines()[0]}
# Free-running analogue-style oscillator phase is not saved; state recall is
# checked by the host's actual parameters, not a false bit-identical-audio claim.
ratio=10**((report['recall']['rms_dbfs']-report['initial']['rms_dbfs'])/20)
assert 0.4<ratio<2.5, f'Unexpected level change after reopening: {ratio}'
(out/'reaper-result.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
