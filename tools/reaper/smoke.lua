-- Runs only in an isolated REAPER instance created by test-reaper-windows.ps1.
-- Real VST3 scan/load, editor creation, parameter recall and offline host rendering.
local out = assert(os.getenv('PUNK_FISH_TEST_OUT'), 'Missing isolated test output path')
local phase = assert(os.getenv('PUNK_FISH_TEST_PHASE'), 'Missing test phase')
local sep = package.config:sub(1,1)
local function path(name) return out .. sep .. name end
local function write(name, text)
  local f = assert(io.open(path(name), 'w')); f:write(text); f:close()
end
local function fail(message)
  write('failed-' .. phase .. '.txt', tostring(message))
  reaper.ShowConsoleMsg('PUNK FISH TEST FAILED: ' .. tostring(message) .. '\n')
end
local function test()
  local project = path('Punk-Fish-REAPER-test.rpp')
  local track, fx
  if phase == 'initial' then
    reaper.InsertTrackAtIndex(0, true)
    track = assert(reaper.GetTrack(0, 0), 'Track creation failed')
    fx = reaper.TrackFX_AddByName(track, 'VST3i: Punk Fish (DevilAcidAudio)', false, 1)
    if fx < 0 then fx = reaper.TrackFX_AddByName(track, 'Punk Fish', false, 1) end
    assert(fx >= 0, 'Punk Fish VST3 was not discovered or could not be instantiated')
    reaper.SetCurrentBPM(0, 138, false)
    -- A real item ensures the host schedules this track during an offline render.
    -- The plugin's internal-sequencer mode ignores its MIDI notes.
    local item = reaper.CreateNewMIDIItemInProj(track, 0, 8, false)
    local take = assert(reaper.GetActiveTake(item), 'MIDI item creation failed')
    reaper.MIDI_InsertNote(take, false, false, 0, 120, 0, 36, 100, false)
  else
    reaper.Main_openProject(project)
    track = assert(reaper.GetTrack(0, 0), 'Saved track was not restored')
    fx = 0
  end
  local _, name = reaper.TrackFX_GetFXName(track, fx, '')
  assert(name:find('Punk Fish', 1, true), 'Wrong effect loaded: ' .. name)
  local function parameter(wanted)
    for i = 0, reaper.TrackFX_GetNumParams(track, fx) - 1 do
      local _, label = reaper.TrackFX_GetParamName(track, fx, i, '')
      if label == wanted then return i end
    end
    error('Host parameter not found: ' .. wanted)
  end
  local expected = {['Cutoff']=0.38, ['Resonance']=0.91, ['Acid Mode']=1,
                    ['Note Source']=0, ['Clock']=0, ['Run']=1}
  for label, value in pairs(expected) do
    local index = parameter(label)
    if phase == 'initial' then
      assert(reaper.TrackFX_SetParamNormalized(track, fx, index, value), 'Cannot set ' .. label)
    else
      local actual = reaper.TrackFX_GetParamNormalized(track, fx, index)
      assert(math.abs(actual-value) < 0.001, label .. ' failed state recall: ' .. actual)
    end
  end
  reaper.TrackFX_Show(track, fx, 3)
  assert(reaper.TrackFX_GetFloatingWindow(track, fx), 'The native VST3 editor did not open')
  reaper.GetSetProjectInfo(0, 'RENDER_SETTINGS', 0, true)
  reaper.GetSetProjectInfo(0, 'RENDER_BOUNDSFLAG', 0, true)
  reaper.GetSetProjectInfo(0, 'RENDER_STARTPOS', 0, true)
  reaper.GetSetProjectInfo(0, 'RENDER_ENDPOS', 8, true)
  reaper.GetSetProjectInfo(0, 'RENDER_SRATE', 44100, true)
  reaper.GetSetProjectInfo(0, 'RENDER_CHANNELS', 2, true)
  reaper.GetSetProjectInfo(0, 'RENDER_TAILFLAG', 0, true)
  reaper.GetSetProjectInfo_String(0, 'RENDER_FILE', out, true)
  reaper.GetSetProjectInfo_String(0, 'RENDER_PATTERN', 'punk-fish-' .. phase, true)
  -- REAPER's native WAV configuration: 'evaw', 16-bit PCM.
  reaper.GetSetProjectInfo_String(0, 'RENDER_FORMAT', 'ZXZhdxAAAA==', true)
  reaper.Main_SaveProjectEx(0, project, 0)
  write('editor-ready-' .. phase .. '.txt', name .. '\nREAPER ' .. reaper.GetAppVersion())
  local start = reaper.time_precise()
  local function render()
    if reaper.time_precise() - start < 2 then reaper.defer(render); return end
    -- File: Render project, using most recent settings, auto-close render dialog.
    reaper.Main_OnCommand(42230, 0)
    local deadline = reaper.time_precise() + 120
    local lastSize, stable = -1, 0
    local function finish()
      local f = io.open(path('punk-fish-' .. phase .. '.wav'), 'rb')
      local size = 0
      if f then size=f:seek('end'); f:close() end
      if size > 44 and size == lastSize then stable=stable+1 else stable=0 end
      lastSize=size
      if stable >= 20 then
        reaper.Main_SaveProjectEx(0, project, 0)
        write('passed-' .. phase .. '.txt', 'REAPER ' .. reaper.GetAppVersion() .. '\n' .. name ..
              '\nNative editor opened; parameters ' .. (phase == 'initial' and 'set' or 'recalled') ..
              '; host WAV render completed.\n')
        reaper.Main_OnCommand(40004, 0) -- Quit this isolated, saved instance.
      elseif reaper.time_precise() > deadline then
        fail('Host rendering timed out or did not produce a WAV')
      else reaper.defer(finish) end
    end
    reaper.defer(finish)
  end
  reaper.defer(render)
end
local ok, err=xpcall(test, debug.traceback)
if not ok then fail(err) end
