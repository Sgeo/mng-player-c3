#ifdef _MSC_VER
#pragma warning(disable:4786 4503)
#endif

#include "C2eTypes.h"
#include "soundlib.h"
//#include "../general.h"
//#include "../C2eServices.h"
#include "File.h"

#include <math.h>
#include <iostream>
#include <cstring>
#include <algorithm>

#ifdef _MSC_VER
#pragma warning (disable:4786 4503)
#endif


#include <emscripten.h>
WebAudioBuffer::WebAudioBuffer(char *bytes, DWORD numBytes) {
	this->index = EM_ASM_INT({
		let index = SoundlibWebAudio.buffers.length;
		let audioBufferPromise = bytes ? SoundlibWebAudio.audioContext.decodeAudioData(Module.HEAPU8.slice($0, $1).buffer) : Promise.resolve(null);
		let gainNode = new GainNode(SoundlibWebAudio.audioContext);
		//let panNode = new StereoPannerNode(SoundlibWebAudio.audioContext);
		let splitter = new ChannelSplitterNode(SoundlibWebAudio.audioContext, {numberOfChannels: 2});
		let merger = new ChannelMergerNode(SoundlibWebAudio.audioContext, {numberOfInputs: 2});
		let leftGainNode = new GainNode(SoundlibWebAudio.audioContext);
		let rightGainNode = new GainNode(SoundlibWebAudio.audioContext);

		gainNode.connect(splitter);
		splitter.connect(leftGainNode, 0);
		splitter.connect(rightGainNode, 1);
		leftGainNode.connect(merger, 0, 0);
		rightGainNode.connect(merger, 0, 1);
		merger.connect(SoundlibWebAudio.audioContext.destination);
		
		SoundlibWebAudio.buffers.push({
			audioBufferPromise: audioBufferPromise,
			audioBufferNode: null,
			gainNode: gainNode,
			leftGainNode: leftGainNode,
			rightGainNode: rightGainNode,
			playing: false
		});
		return index;
	}, bytes, numBytes);
}

void WebAudioBuffer::Play(bool loop) {
	EM_ASM({
		let buffer = SoundlibWebAudio.buffers[$0];
		buffer.audioBufferPromise = (async function() {
			let audioBuffer = await buffer.audioBufferPromise;
			if(buffer.audioBufferNode) {
				buffer.audioBufferNode.stop();
				buffer.audioBufferNode.disconnect(buffer.gainNode);
				buffer.audioBufferNode = null;
			}
			buffer.audioBufferNode = new AudioBufferSourceNode(SoundlibWebAudio.audioContext);
			buffer.audioBufferNode.addEventListener('ended', () => {
				buffer.playing = false;
			});
			buffer.audioBufferNode.buffer = audioBuffer;
			buffer.audioBufferNode.loop = !!loop;
			buffer.audioBufferNode.connect(buffer.gainNode);
			buffer.audioBufferNode.start();
			return audioBuffer;
		})();
	}, this->index, loop);
}

void WebAudioBuffer::Stop() {
	EM_ASM({
		let buffer = SoundlibWebAudio.buffers[$0];
		buffer.audioBufferPromise = (async function() {
			let audioBuffer = await buffer.audioBufferPromise;
			if(buffer.audioBufferNode) {
				buffer.audioBufferNode.stop();
				buffer.audioBufferNode.disconnect(buffer.gainNode);
				buffer.audioBufferNode = null;
			}
			return audioBuffer;
		})();
	}, this->index);
}

void WebAudioBuffer::SetVolume(long lVolume) {
	float gain = powf(10.0f, (float)lVolume / 2000.0f);

	EM_ASM({
		SoundlibWebAudio.buffers[$0].gainNode.gain.value = $1;
	}, this->index, gain);

}

void WebAudioBuffer::SetPan(long lPan) {
	// Remember that 0 is the highest volume
	long lVolLeft  = std::min(0L, -lPan);
	long lVolRight = std::min(0L,  lPan);
	float leftGain = powf(10.0f, (float)lVolLeft / 2000.0f);
	float rightGain = powf(10.0f, (float)lVolRight / 2000.0f);
	EM_ASM({
		SoundlibWebAudio.buffers[$0].leftGainNode.gain.value = $1;
		SoundlibWebAudio.buffers[$0].rightGainNode.gain.value = $2;
	}, this->index, leftGain, rightGain);
}

WebAudioBuffer *WebAudioBuffer::Duplicate() {
	WebAudioBuffer *duplicate = new WebAudioBuffer(nullptr, 0);
	EM_ASM({
		let source = SoundlibWebAudio.buffers[$0];
		let dest = SoundlibWebAudio.buffers[$1];
		dest.audioBufferPromise = source.audioBufferPromise;
		dest.gainNode.gain.value = source.gainNode.gain.value;
		dest.leftGainNode.gain.value = source.leftGainNode.gain.value;
		dest.rightGainNode.gain.value = source.rightGainNode.gain.value;
	}, this->index, duplicate->index);
	return duplicate;
}

bool WebAudioBuffer::IsPlaying() {
	return EM_ASM_INT({
		return SoundlibWebAudio.buffers[$0].playing ? 1 : 0;
	}, this->index) != 0;
}

WebAudioBuffer::~WebAudioBuffer() {
	// TODO: Does anything need to be done here?
}


// ----------------------------------------------------------------------
// History:
// 05May98 Peter Chilvers Altered OpenSound and GetWaveSize to handle
//						  Playing from the "munged" music file
//						  Commented out DSBSTATIC flag in PlaySound
// 26May98 Peter Chilvers Moved initialisation into constructor head
//						  Added FadeIn and FadeOut
// 29May98 Peter Chilvers Fixed minimum volume to -5000
// 
// ----------------------------------------------------------------------

CachedSound::CachedSound()
{
	name=0;
	size=0;
	used=0;
	copies=0;
	buffer=NULL;
}

CachedSound::~CachedSound()
{
	if (buffer!=NULL)
	{
		// Sound buffer not released (this was a log statement)
		ASSERT(false);

		delete buffer;
	}
}



ActiveSample::ActiveSample() :
	pSample(NULL),
	wID(0),
	cloned(NULL),
	fade_rate(0),
	locked(FALSE),
	volume(0)
{
}


// Static variables
// Share the DirectSound object and primary buffers between different sound managers
int	SoundManager::references=0;

void SoundManager::SetMNGFile(std::string& mng)
{
	if (mungeFile == mng)
		return;
	mungeFile = mng;
	FlushCache();
}

SoundManager::SoundManager() :
	maximum_size(0),
	current_size(0),
	last_used(0),
	sound_initialised(FALSE),
	overall_volume(0),
	target_volume(0),
	current_volume(0),
	faded(false)
{

	EM_ASM({
		SoundlibWebAudio = {};
		SoundlibWebAudio.buffers = [];
		SoundlibWebAudio.audioContext = new AudioContext();
	});

	mungeFile = "music.mng";

	references++;

	// Only set up the direct sound object the first time
	if (references == 1)
		{
		///////////////////////////////////////////////////
		// SET UP DIRECT SOUND (FOR EFFECTS)

		
		//pPrimary = nullptr; // TODO: ?


		}
	sounds_playing = 0;
	sound_index = 1;

	//initialize the sample slots...
	for(int i=0;i<MAX_ACTIVE_SOUNDS;i++)
	{
		active_sounds[i].wID = 0;
	}


	///////////////////////////////////////////////////
	// what we could also do here is to set up the PRIMARY buffer and play it - this would reduce the startup time
	// when the first secondary buffer is played.

	sound_initialised=TRUE;

	// Mixer is active, but silent

	mixer_suspended=FALSE;



}

SoundManager::~SoundManager()
{

	if (!sound_initialised)
	{
		return;
	}

	StopAllSounds();

	FlushCache();	

	// Only close down if this is the last remaining sound manager

	if (-- references == 0)
		{


		}
}

CachedSound *SoundManager::OpenSound(DWORD wave)
{
	if (!sound_initialised)
	{
		return (NULL);
	}

	// Either read from the munged file, or
	// from the sound effects file
	bool munged = wave >= 0xff000000;

	File file;
	int offset, size;
	if (munged)
		{
		try
		{
			char buf[_MAX_PATH] = "/home/web_user/music/";
			std::string path(buf);
			//path+="Music.mng";
			path += mungeFile;
			file.Open(path);
		
			if(!file.Valid())
			{
				return NULL;
			}
		}
		catch(File::FileException& e)
		{
			//ErrorMessageHandler::Show(e, std::string("SoundLib::OpenSound"));
			std::cerr << "SoundLib::OpenSound\n" << e.what() << "\n";
			return NULL;

		}


		// Calculate the index into the file
		DWORD index = wave - 0xff000000;

		// Skip straight to the appropriate offset

		// The header consists of:
		// total voices
		// offset to script, sizeof script
		// offset to wave, sizeof wave
		// etc.

		file.Seek( (3 + index * 2) * sizeof(int), File::Start);

		// Now read in the offset to the start, and the total size
		
		file.Read(&offset,sizeof(int));
		file.Read(&size,sizeof(int));

		// Now point straight to the start of the wave
		file.Seek(offset,File::Start);
		}
	else
	{
		std::cerr << "Not supporting sound effects right now, sorry!\n";
		return NULL;
	}

	int wav_size = size+16;
	char *wav_data = new char[wav_size];
	strncpy(wav_data, "RIFF\x00\x00\x00\x00WAVEfmt ", 16);
	wav_data[7] = ((wav_size-8) >> 24) & 0xFF;
	wav_data[6] = ((wav_size-8) >> 16) & 0xFF;
	wav_data[5] = ((wav_size-8) >> 8) & 0xFF;
	wav_data[4] = ((wav_size-8)) & 0xFF;

	file.Read(wav_data+16, size);

	WebAudioBuffer *buffer = new WebAudioBuffer(wav_data, wav_size);
	CachedSound *w = new CachedSound;
	w->name = wave;
	w->used = last_used++;
	w->size = size;
	w->copies = 0;
	w->buffer = buffer;

	return w;
	
}

SOUNDHANDLE SoundManager::PlayCachedSound(CachedSound *wave,
				int volume, int pan, BOOL loop)
{
	WebAudioBuffer *pDSBuffer=wave->buffer;
	ActiveSample		*pActive;
	//------------------

	//any spare slots????
	if(!sound_initialised || sounds_playing==MAX_ACTIVE_SOUNDS)
		{
		// Sound buffer full;
		return -1;
		}

	if (sounds_playing>MAX_ACTIVE_SOUNDS)
	{
		// Active sounds overflowed limit
		return -1;
	}

	//is it a valid sample 
	//....

	//find a spare slot
	int i=0;
	while(active_sounds[i].wID!=0 && i<MAX_ACTIVE_SOUNDS)
	{
		i++;
	}

	pActive=&active_sounds[i];

	// Set the sound's basic volume (before overall volume is taken
	// into account)
	pActive->volume = volume;
	
	//is the bank's sample playing??? if it is we need to duplicate it...

	
	if(pDSBuffer->IsPlaying())
	{
		pActive->pSample = pDSBuffer->Duplicate();
		
		pActive->cloned=wave;	//mark as not the original buffer i.e. a copy
		wave->copies++;
	}
	else
	{
		pActive->pSample=pDSBuffer;
		pActive->cloned=NULL;	//mark AS the original buffer

	}



	pDSBuffer=pActive->pSample;

	// Take the overall volume of the manager into account
	volume += current_volume;

	// Can't get quieter than silence
	if (volume < SoundMinVolume)
		{
		volume = SoundMinVolume;
		}

	pDSBuffer->SetVolume(volume);

	if (volume > 0 || volume < SoundMinVolume)
		{
		// "Volume out of range %d\n",volume 
		}

	pDSBuffer->SetPan(pan);

	//play the sample 
	
	pDSBuffer->Play(loop);	

	//set up the sample
	pActive->wID= ++sound_index;			//need pre increment!

	sounds_playing++;

	// Flag as unlocked (delete when finished)
	active_sounds[i].locked=FALSE;
	active_sounds[i].fade_rate=0;

	return (i);		// return channel number (SOUNDHANDLE)

}

void SoundManager::StopAllSounds()
{
	if (sound_initialised)
	{
		FlushSoundQueue();
		if (sounds_playing)
		{
			for(int i=0;i<MAX_ACTIVE_SOUNDS;i++)
			{
				StopSound(i);
			}
		}
	}
}

void SoundManager::FlushSoundQueue()
{
	std::vector<SoundQueueItem*>::iterator it;
		// Flush existing cache
	for (it = sound_queue.begin(); it != sound_queue.end(); it++)
	{
		//	deallocate wave
		delete (*it);
		(*it)=NULL;
	}

	sound_queue.clear();
	
}

void SoundManager::UpdateSoundQueue()					
{
	// Scan through sound queue backwards, flagging that a tick has
	// passed, and playing sounds whose 'time has come'

//	for (int i=sound_queue.GetSize()-1;i>=0;i--)
	std::vector<SoundQueueItem*>::iterator item;

	for (item = sound_queue.begin(); item != sound_queue.end(); item++)
	{
		if((*item))
			{
			if ((*item)->ticks<=0)
			{
			
				PlaySoundEffect((*item)->wave, 0, (*item)->volume, (*item)->pan);
				delete (*item);
				*item = NULL;
			}
			else
			{
				(*item)->ticks--;
			}
		}
	}
	
}

										

									
void SoundManager::Update()
	{
	const long MANAGER_FADE_RATE = 200;

	if (sound_initialised)
		{
		// Play any delayed sounds that are waiting
		UpdateSoundQueue();

		// Move the played volume towards the target volume
		if (current_volume < target_volume)
			{
			current_volume += MANAGER_FADE_RATE;

			// check we didn't overshoot
			if (current_volume > target_volume)
				{
				current_volume = target_volume;
				}
			}
		else
			{
			if (current_volume > target_volume)
				{
				current_volume -= MANAGER_FADE_RATE;

				// check we didn't overshoot
				if (current_volume < target_volume)
					{
					current_volume = target_volume;
					}
				}
			}


		if (sounds_playing)
			{
			for(int i=0;i<MAX_ACTIVE_SOUNDS;i++)
				{
				if (active_sounds[i].wID)
					{
					DWORD status;
					WebAudioBuffer *pDSBuffer=active_sounds[i].pSample;
					if (!(pDSBuffer->IsPlaying()))
						{
						if (!active_sounds[i].locked)
							{
							StopSound(i);
							}
						}
					else
						{
						// Is the sound fading?
						if (active_sounds[i].fade_rate)
							{
							// Decrease the sound's basic volume by 
							// the fade rate
							active_sounds[i].volume += active_sounds[i].fade_rate;

							// Calculate the volume of the buffer, taking
							// into account the overall volume of the manager
							long volume = active_sounds[i].volume + current_volume;

							// Is the sound still audible ?
							if (volume<=SoundMinVolume)
								{
								// No - stop it
								StopSound(i);
								}
							else
								{
								// Adjust the buffer to take the overall
								// volume into account
								pDSBuffer->SetVolume(volume);
								
								if (volume > 0 || volume < SoundMinVolume)
									{
									// "Volume out of range %d\n",volume
									}
								}
							}
						else
							{
							// Calculate the volume of the buffer, taking
							// into account the overall volume of the manager
							long volume = active_sounds[i].volume + current_volume;

							// Can't get quieter than silence
							if (volume < SoundMinVolume)
								{
								volume = SoundMinVolume;
								}

							pDSBuffer->SetVolume(volume);

							if (volume > 0 || volume < SoundMinVolume)
								{
								// "Volume out of range %d\n",volume
								}


							}

						}
					}
				}
			}

		}
	}

void SoundManager::StopSound(SOUNDHANDLE handle)
{
	if (sounds_playing && sound_initialised)
	{
		ActiveSample *pActive=&active_sounds[handle];
		if (pActive->wID!=0)
		{
			pActive->wID=0;
			// alima added this
			// when you call stop on a secondary buffer
			// the sample will remember where it stopped
			// and will continue playing from there when you
			// restart it.   
			// but that would be pause sound so I will
			// set it back to the beginning here...
			pActive->pSample->Stop();
			//pActive->pSample->SetCurrentPosition(0);


			if (pActive->cloned!=NULL)
			{
				pActive->pSample->Release();
				pActive->cloned->copies--;
			}

			sounds_playing--;
		}
	}
}

void SoundManager::CloseSound(CachedSound *wave)
{
	if (wave==NULL)
	{
		return;
	}

	if (wave->copies>0)
	{
		// stop and remove any copies still playing
		for(int i=0;i<MAX_ACTIVE_SOUNDS;i++)
		{
			if (active_sounds[i].wID && active_sounds[i].cloned==wave)
			{
				StopSound(i);
			}
		}
	}

	if (wave->buffer!=NULL)
	{
		// now remove the original if it is still playing
		for(int i=0;i<MAX_ACTIVE_SOUNDS;i++)
		{
			if (active_sounds[i].wID && active_sounds[i].pSample==wave->buffer)
			{
				StopSound(i);
			}
		}
		wave->buffer->Release();
		wave->buffer=NULL;
	}

	delete wave;

}

BOOL SoundManager::SoundEnabled()					//  Is mixer running?
{
	return(sound_initialised && !mixer_suspended);
}

SOUNDERROR SoundManager::InitializeCache(int size)	//	Set size (K) of cache in bytes
													//  Flushes the existing cache
{
	FlushCache();

	maximum_size=size*1024;		// Fixes size of cache (in bytes)

	current_size=0;
	last_used=0;

	return(NO_SOUND_ERROR);
}

SOUNDERROR SoundManager::FlushCache()	//  Clears all stored sounds from
										//  the sound cache
{
	if (sound_initialised)
	{
		StopAllSounds();

		if (maximum_size>0)
		{	
			// Flush existing cache
		//	for (int i=0;i<sounds.GetSize();i++)
		//	{
			std::set<CachedSound*>::iterator item;

			for (item = sounds.begin(); item != sounds.end(); item++)
			{
			//	deallocate wave
			//	CachedSound *wave=(CachedSound *) sounds[i];
				CloseSound((*item));
			}

			sounds.clear();
		}
	}
	current_size=0;

	return(NO_SOUND_ERROR);
}

SOUNDERROR SoundManager::SuspendMixer()				//  Stop the mixer playing
													//  (Use on KillFocus)
{
	if (mixer_suspended || !sound_initialised)
	{
		return(SOUND_MIXER_SUSPENDED);
	}

	StopAllSounds();	

	mixer_suspended=TRUE;

	return(NO_SOUND_ERROR);

}

SOUNDERROR SoundManager::RestoreMixer()		//  Restart the mixer
											//  After it has been suspended

{
	if (mixer_suspended && sound_initialised)
	{
		mixer_suspended=FALSE;
	}
	return(NO_SOUND_ERROR);
}

SOUNDERROR SoundManager::RemoveFromCache(CachedSound* index)
{
	std::set<CachedSound*>::iterator it = sounds.find(index);

	if(it!= sounds.end())
	{
		current_size-=(*it)->size;
		CloseSound((*it));

		sounds.erase(it);
		return(NO_SOUND_ERROR);
	}
	else
	{
		return(SOUND_NOT_FOUND);
	}
/*
	if (index>sounds.size())
	{
		return(SOUND_NOT_FOUND);
	}
	else
	{
		CachedSound *w=(CachedSound *) sounds[index];

		current_size-=w->size;
		CloseSound(w);

		
		sounds.RemoveAt(index);
		return(NO_SOUND_ERROR);
	}*/
}

SOUNDERROR SoundManager::MakeRoomInCache(int size)
{
	// Is sound too big for cache?
	
	if (maximum_size<size)
	{
		return(SOUNDCACHE_TOO_SMALL);
	}

	// We need to clear enough space for the sound

	BOOL no_space=FALSE;	// TRUE if nothing can be deleted

	while((maximum_size-current_size)<size && !no_space)
	{
		// Repeatedly remove least used sound
		// until there is adequate room

		int age=0x7FFFFFFF;
		int oldest=-1;

		CachedSound * oldestWave = NULL;
		std::set<CachedSound*>::iterator it;

	//	for(int i=0;i<sounds.size();i++)
		for(it = sounds.begin(); it!=sounds.end();it++)
		{
		
			if ((*it)->used<age)
			{
			
				// is this sound playing, or is there an existing clone playing?
				if (!((*it)->buffer->IsPlaying()) && (*it)->copies==0)
				{
					age=(*it)->used;
				oldestWave = (*it);
				}
			}
		}

		if (oldestWave)
		{
			RemoveFromCache(oldestWave);
		}
		else
		{
			no_space=TRUE;
		}
	}



	return(NO_SOUND_ERROR);

}

int SoundManager::GetWaveSize(DWORD wave)
{
	// Is this one of the munged files ?
	if (wave >= 0xff000000)
		{	
		// Yes - read in its details from the munged file
		File munged;
		try
		{
			char buf[_MAX_PATH] = "/home/web_user/music";
			std::string path(buf);
			//path+="Music.mng";
			path += mungeFile;
			munged.Open(path);
			

			// Calculate the index into the file
			uint32 index = wave - 0xff000000;

			// Skip straight to the appropriate offset

			// The header consists of:
			// total voices
			// offset to script, sizeof script
			// offset to wave, sizeof wave
			// etc.
			munged.Seek(( (index * 2) + 4 ) * sizeof(int), File::Start);

			int size;
			munged.Read(&size,sizeof(int));

			return size;
			}
		catch(File::FileException& e)
			{
			//ErrorMessageHandler::Show(e, std::string("SoundLib::GetWaveSize"));
			std::cerr << "SoundLib::GetWaveSize\n" << e.what() << "\n";
			return (0);
			}
		}
	else
	{
		std::cerr << "Non-munged files not supported\n";
		return 0;
		// return 0;
		// File file;
		// try
		// {
		// 	// Need to load in sound from 'Sounds' directory
		// 	file.Open(std::string(BuildFsp(wave,"wav",SOUNDS_DIR)));
		// }
		// catch(File::FileException& e)
		// {
		// 	//ErrorMessageHandler::Show(e, std::string("SoundLib::GetWaveSize"));
		// 	std::cerr << "SoundLib::GetWaveSize\n" << e.what() << "\n";
		// 	return(0);
		// }
		
		// return(file.GetSize());
		
	}
}


CachedSound *	SoundManager::EnsureInCache(DWORD wave)			//  Places wave in cache and returns
{

	// Is the sound already in the cache?
	BOOL found=FALSE;
	int i=0;
		
	std::set<CachedSound*>::iterator it;

	for(it = sounds.begin(); it!=sounds.end();it++)
	{

		if ((*it)->name==wave)
		{
			break;//=TRUE;		// we have found the sound at the correct volume
		}
	}

	if (it != sounds.end())
	{
		(*it)->used=last_used++;
		return(*it);
	}

	
	
	// Clear space to load sound (taking .wav header into account)

	if (MakeRoomInCache(GetWaveSize(wave))==NO_SOUND_ERROR)
	{

		CachedSound *new_wave=OpenSound(wave);

		if (new_wave!=NULL)
			{

			// Add to cache
			sounds.insert(new_wave);

			// Increase 
			current_size+=new_wave->size;
			}
		else
			{
			// "Sound not found: %s\n",BuildFsp(wave,"wav")
			}
		return(new_wave);
	}
	else
	{
		
		// "Cache Full: Couldn't play %s\n",BuildFsp(wave,"wav"));
		return(NULL);
	}
}

SOUNDERROR SoundManager::PreLoadSound(DWORD wave)	//  Ensures a sound is loaded into the
																	//  cache
{
	if (mixer_suspended || !sound_initialised)
	{
		return(SOUND_MIXER_SUSPENDED);
	}
	
	CachedSound *w=EnsureInCache(wave);

	if (w==NULL)
	{
		return(SOUND_NOT_FOUND);
	}
	else
	{
		return(NO_SOUND_ERROR);
	}
}

SOUNDERROR SoundManager::PlaySoundEffect(DWORD wave, int ticks, long volume, long pan)		//  Loads in sound or locates within
										//  Sound cache and plays it.
										//  No further control
{
	if (mixer_suspended || !sound_initialised)
	{
		return(SOUND_MIXER_SUSPENDED);
	}

	if (ticks)
	{
		// Queue sound to be played later
		
		SOUNDERROR err;
		err=PreLoadSound(wave);
		if (err==NO_SOUND_ERROR)
		{

			SoundQueueItem *add=new SoundQueueItem;

			add->wave=wave;
			add->ticks=ticks;
			add->volume=volume;
			add->pan=pan;
		
			sound_queue.push_back(add);
		}

		return(err);
	}

	// Place in cache and play now

	CachedSound *w=EnsureInCache(wave);
	if (w!=NULL)
	{
		SOUNDHANDLE handle=PlayCachedSound(w,volume,pan);
		if (handle==-1)
		{
			return(SOUND_NOT_FOUND);
		}
		else
		{
			return(NO_SOUND_ERROR);
		}
		return(NO_SOUND_ERROR);
	}
	else
	{
		return(SOUND_NOT_FOUND);
	}
}

SOUNDERROR SoundManager::StartControlledSound(DWORD wave, SOUNDHANDLE &handle, long volume, long pan, BOOL looped)
{
	if (mixer_suspended || !sound_initialised)
	{
		// --->SOUND_MIXER_SUSPENDED
		return(SOUND_MIXER_SUSPENDED);
	}
	
	CachedSound *w=EnsureInCache(wave);
	if (w!=NULL)
	{
		handle=PlayCachedSound(w,volume,pan,looped);
		if (handle==-1)
		{
			// Too many sounds playing: Couldn't play %s\n",BuildFsp(wave,"wav"))
			return(SOUND_NOT_FOUND);
		}
		else
		{
			// Flag as locked (don't delete until instructed)
			active_sounds[handle].locked=TRUE;
			return(NO_SOUND_ERROR);
		}
	}
	else
	{
		return(SOUND_NOT_FOUND);
	}

}

SOUNDERROR SoundManager::UpdateControlledSound(SOUNDHANDLE handle, long volume, long pan)
{
	if (mixer_suspended)
	{
		return(SOUND_MIXER_SUSPENDED);
	}

	if (handle<0 || handle>=MAX_ACTIVE_SOUNDS)
	{
		return(SOUND_HANDLE_UNDEFINED);
	}

	// Store the basic volume of the sound
	active_sounds[handle].volume = volume;


	// Calculate the volume of the buffer, taking
	// into account the overall volume of the manager
	volume += current_volume;

	// Can't get quieter than silence
	if (volume < SoundMinVolume)
		{
		volume = SoundMinVolume;
		}

	active_sounds[handle].pSample->SetVolume(volume);
	if (volume > 0 || volume < SoundMinVolume)
		{
		// Volume out of range %d\n",volume
		}
	active_sounds[handle].pSample->SetPan(pan);

	return(NO_SOUND_ERROR);
}

BOOL SoundManager::FinishedControlledSound(SOUNDHANDLE handle)
{
	if (mixer_suspended)
	{
		return(TRUE);
	}

	if (handle<0 || handle>=MAX_ACTIVE_SOUNDS)
	{
		return(TRUE);
	}

	if (active_sounds[handle].wID)
	{
		DWORD status;
		WebAudioBuffer *pDSBuffer=active_sounds[handle].pSample;
		if (pDSBuffer->IsPlaying())
		{
			return(FALSE);
		}

	}

	return(TRUE);
}

SOUNDERROR SoundManager::StopControlledSound(SOUNDHANDLE handle, BOOL fade)
{

	if (mixer_suspended)
	{
		return(SOUND_MIXER_SUSPENDED);
	}

	if (handle<0 || handle>=MAX_ACTIVE_SOUNDS)
	{
		return(SOUND_HANDLE_UNDEFINED);
	}

	active_sounds[handle].locked=FALSE;

	if (fade)
	{
		// fade out in 15 ticks
		active_sounds[handle].fade_rate=(SoundMinVolume-active_sounds[handle].volume)/15;

		if (active_sounds[handle].fade_rate >= 0)
			{
			// No point in fading, as the sound is already silent
			// Zero fade rate - sound stopped
			StopSound(handle);
			}

	}
	else
	{
	 	StopSound(handle);
	}

	return(NO_SOUND_ERROR);
}

SOUNDERROR SoundManager::SetVolume(long volume)
	{
	if (mixer_suspended)
		{
		return(SOUND_MIXER_SUSPENDED);
		}

	overall_volume = volume;

	// If we're currently audible...
	if (!faded)
		{
		// ... Fade towards the new volume
		target_volume = overall_volume;
		}

	return(NO_SOUND_ERROR);

	}

SOUNDERROR SoundManager::SetTargetVolume(long volume)
	{
	if (mixer_suspended)
		{
		return(SOUND_MIXER_SUSPENDED);
		}

	target_volume = volume;

	return(NO_SOUND_ERROR);

	}

SOUNDERROR SoundManager::FadeOut()
	{
	if (mixer_suspended)
		{
		return(SOUND_MIXER_SUSPENDED);
		}

	// Aim for complete silence
	target_volume = SoundMinVolume;

	// Flag that we are fading out
	faded = true;

	return(NO_SOUND_ERROR);

	}

SOUNDERROR SoundManager::FadeIn()
	{
	if (mixer_suspended)
		{
		return(SOUND_MIXER_SUSPENDED);
		}

	// Aim for maximum volume
	target_volume = overall_volume;

	// Flag that we are fading in
	faded = false;

	return(NO_SOUND_ERROR);

	}


