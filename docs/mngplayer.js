import MNGPlayerSys from './mngplayer-sys.js';

export default class MNGPlayer {
  async load(url) { // TODO: How to present secondary .mngs, especially if it's hard to retrieve track names
    this._mngplayer = await MNGPlayerSys();
    let mngdata = await fetch(url).then(resp => resp.arrayBuffer()).then(arrayBuffer => new Uint8Array(arrayBuffer));
    this._mngplayer.FS.mkdir("/home/web_user/music/");
    this._mngplayer.FS.writeFile("/home/web_user/music/music.mng", mngdata);
    this._musicmanager = new this._mngplayer.MusicManager();
    this._musicmanager.LoadScrambled();
  }

  getTrackNames() {
    let result = [];
    let numTracks = this._musicmanager.NumberOfTracks();
    this._musicmanager.StartReadingTracks();
    for(let i = 0; i < numTracks; i++) {
      result.push(this._musicmanager.GetNextTrack());
    }
    return result;
  }

  get threat() {
    return this._musicmanager.GetThreat();
  }
  set threat(new_threat) {
    this._musicmanager.UpdateSettings(this.mood, new_threat);
  }

  get mood() {
    return this._musicmanager.GetMood();
  }
  set mood(new_mood) {
    this._musicmanager.UpdateSettings(new_mood, this.threat);
  }

  beginTrack(track) {
    this._musicmanager.BeginTrack(track);
  }

  interruptTrack(track) { // Use misspelling here?
    this._musicmanager.InteruptTrack(track);
  }

  play() {
    this._musicmanager.Play();
  }

  pause() {
    this._musicmanager.Pause();
  }

  fade() {
    this._musicmanager.Fade();
  }

  get volume() {
    return this._musicmanager.GetVolume();
  }
  set volume(new_volume) {
    this._musicmanager.SetVolume(new_volume);
  }

  get playing() {
    return this._musicmanager.IsPlaying();
  }

  run(ms) {
    return setInterval(() => {
      this._musicmanager.getSoundManager().Update();
      this._musicmanager.Update();
    }, ms ?? 50)
  }
}