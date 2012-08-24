#ifndef LOOPBACK_CAPTURE_HPP__
#define LOOPBACK_CAPTURE_HPP__

#include <cstdint>
#include <tuple>

#include <audioclient.h>
#include <avrt.h>

#include "generic_guard.hpp"


HRESULT write_wave_header(HMMIO file_handle_, LPCWAVEFORMATEX pwfx, MMCKINFO *pckRIFF, MMCKINFO *pckData) 
{
	MMRESULT result;

	// make a RIFF/WAVE chunk
	pckRIFF->ckid = MAKEFOURCC('R', 'I', 'F', 'F');
	pckRIFF->fccType = MAKEFOURCC('W', 'A', 'V', 'E');

	result = mmioCreateChunk(file_handle_, pckRIFF, MMIO_CREATERIFF);
	if (MMSYSERR_NOERROR != result) 
	{
		printf("mmioCreateChunk(\"RIFF/WAVE\") failed: MMRESULT = 0x%08x\n", result);
		return E_FAIL;
	}

	// make a 'fmt ' chunk (within the RIFF/WAVE chunk)
	MMCKINFO chunk;
	chunk.ckid = MAKEFOURCC('f', 'm', 't', ' ');
	result = mmioCreateChunk(file_handle_, &chunk, 0);
	if (MMSYSERR_NOERROR != result)
	{
		printf("mmioCreateChunk(\"fmt \") failed: MMRESULT = 0x%08x\n", result);
		return E_FAIL;
	}

	// write the WAVEFORMATEX data to it
	LONG lBytesInWfx = sizeof(WAVEFORMATEX) + pwfx->cbSize;
	LONG lBytesWritten =
		mmioWrite(
		file_handle_,
		reinterpret_cast<PCHAR>(const_cast<LPWAVEFORMATEX>(pwfx)),
		lBytesInWfx
		);
	if (lBytesWritten != lBytesInWfx)
	{
		printf("mmioWrite(fmt data) wrote %u bytes; expected %u bytes\n", lBytesWritten, lBytesInWfx);
		return E_FAIL;
	}

	// ascend from the 'fmt ' chunk
	result = mmioAscend(file_handle_, &chunk, 0);
	if (MMSYSERR_NOERROR != result)
	{
		printf("mmioAscend(\"fmt \" failed: MMRESULT = 0x%08x\n", result);
		return E_FAIL;
	}

	// make a 'fact' chunk whose data is (DWORD)0
	chunk.ckid = MAKEFOURCC('f', 'a', 'c', 't');
	result = mmioCreateChunk(file_handle_, &chunk, 0);
	if (MMSYSERR_NOERROR != result) 
	{
		printf("mmioCreateChunk(\"fmt \") failed: MMRESULT = 0x%08x\n", result);
		return E_FAIL;
	}

	// write (DWORD)0 to it
	// this is cleaned up later
	DWORD frames = 0;
	lBytesWritten = mmioWrite(file_handle_, reinterpret_cast<PCHAR>(&frames), sizeof(frames));
	if (lBytesWritten != sizeof(frames))
	{
		printf("mmioWrite(fact data) wrote %u bytes; expected %u bytes\n", lBytesWritten, (uint32_t)sizeof(frames));
		return E_FAIL;
	}

	// ascend from the 'fact' chunk
	result = mmioAscend(file_handle_, &chunk, 0);
	if (MMSYSERR_NOERROR != result) 
	{
		printf("mmioAscend(\"fact\" failed: MMRESULT = 0x%08x\n", result);
		return E_FAIL;
	}

	// make a 'data' chunk and leave the data pointer there
	pckData->ckid = MAKEFOURCC('d', 'a', 't', 'a');
	result = mmioCreateChunk(file_handle_, pckData, 0);
	if (MMSYSERR_NOERROR != result) 
	{
		printf("mmioCreateChunk(\"data\") failed: MMRESULT = 0x%08x\n", result);
		return E_FAIL;
	}

	return S_OK;
}

HRESULT finish_wave_file(HMMIO file_handle_, MMCKINFO *pckRIFF, MMCKINFO *pckData) 
{
	MMRESULT result;

	result = mmioAscend(file_handle_, pckData, 0);
	if (MMSYSERR_NOERROR != result) 
	{
		printf("mmioAscend(\"data\" failed: MMRESULT = 0x%08x\n", result);
		return E_FAIL;
	}

	result = mmioAscend(file_handle_, pckRIFF, 0);
	if (MMSYSERR_NOERROR != result) 
	{
		printf("mmioAscend(\"RIFF/WAVE\" failed: MMRESULT = 0x%08x\n", result);
		return E_FAIL;
	}

	return S_OK;    
}



// call CreateThread on this function
// feed it the address of a LoopbackCaptureThreadFunctionArguments
// it will capture via loopback from the IMMDevice
// and dump output to the HMMIO
// until the stop event is set
// any failures will be propagated back via hr



//TODO: puede ser static o free function
//TODO: quien es responsable de hacer release?
void int16_processing( WAVEFORMATEX* pwfx)		//TODO: pointer to reference
{
	// coerce int-16 wave format
	// can do this in-place since we're not changing the size of the format
	// also, the engine will auto-convert from float to int for us
	switch (pwfx->wFormatTag) 
	{
		case WAVE_FORMAT_IEEE_FLOAT:
			pwfx->wFormatTag = WAVE_FORMAT_PCM;
			pwfx->wBitsPerSample = 16;
			pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
			pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
			break;

		case WAVE_FORMAT_EXTENSIBLE:
			{
				// naked scope for case-local variable
				PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
				if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) 
				{
					pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
					pEx->Samples.wValidBitsPerSample = 16;
					pwfx->wBitsPerSample = 16;
					pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
					pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
				} 
				else 
				{
					printf("Don't know how to coerce mix format to int-16\n");
					//CoTaskMemFree(pwfx);
					//return E_UNEXPECTED;
					throw 1; //TODO:
				}
			}
			break;

		default:
			printf("Don't know how to coerce WAVEFORMATEX with wFormatTag = 0x%08x to int-16\n", pwfx->wFormatTag);
			//CoTaskMemFree(pwfx);
			//return E_UNEXPECTED;
			throw 1; //TODO:
	}
}

//TODO: puede ser static o free function
//TODO: rename function name
//TODO: const correctness
std::tuple<MMCKINFO, MMCKINFO, uint32_t> inner_work_1( HMMIO file_handle, IAudioClient *audio_client, bool is_int16 )
{
	HRESULT hr;

	// get the default device format
	WAVEFORMATEX* pwfx;
	hr = audio_client->GetMixFormat(&pwfx);

	auto wave_formatex_releaser = finally ([&] { 
		CoTaskMemFree(pwfx);
	});

	if ( FAILED(hr) ) 
	{
		printf("IAudioClient::GetMixFormat failed: hr = 0x%08x\n", hr);
		//CoTaskMemFree(pwfx);
		//return;
		throw 1;	//TODO:
	}

	if ( is_int16 )
	{
		int16_processing(pwfx);
	}

	MMCKINFO ckRIFF = {0};	//TODO: rename
	MMCKINFO ckData = {0};	//TODO: rename
	hr = write_wave_header(file_handle, pwfx, &ckRIFF, &ckData);
	if (FAILED(hr)) 
	{
		// write_wave_header does its own logging
		//CoTaskMemFree(pwfx);
		//return;
		throw 1;	//TODO:
	}

	uint32_t block_align = pwfx->nBlockAlign;

	// call IAudioClient::Initialize
	// note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK
	// do not work together...
	// the "data ready" event never gets set
	// so we're going to do a timer-driven loop
	hr = audio_client->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_LOOPBACK,
		0, 0, pwfx, 0
		);
	if (FAILED(hr)) 
	{
		printf("IAudioClient::Initialize failed: hr = 0x%08x\n", hr);
		//return;
		throw 1;	//TODO:
	}
	//CoTaskMemFree(pwfx);

	return std::make_tuple(ckRIFF, ckData, block_align);
}

//TODO: retornar un smart_ptr o un guard con auto release algo similar a unique_ptr pero con el deleter seteable con lambda.
IAudioClient* get_active_audio_client( IMMDevice* mm_device )
{
	HRESULT hr;

	IAudioClient* audio_client;
	hr = mm_device->Activate( __uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audio_client );

	if ( FAILED(hr) )
	{
		printf("IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x", hr);
		throw 1; //TODO:
	}

	return audio_client;
}


REFERENCE_TIME get_default_device_period( IAudioClient* audio_client )
{
	// get the default device periodicity

	HRESULT hr;

	REFERENCE_TIME default_device_period;		//__int64
	hr = audio_client->GetDevicePeriod(&default_device_period, NULL);			// The time is expressed in 100-nanosecond units ( 1 millisecond = 1 000 000 nanoseconds )
	if (FAILED(hr)) 
	{
		printf("IAudioClient::GetDevicePeriod failed: hr = 0x%08x\n", hr);
		//return;
		throw 1; //TODO:
	}

	return default_device_period;
}

IAudioCaptureClient* get_active_audio_capture_client( IAudioClient* audio_client )
{
	HRESULT hr;

	IAudioCaptureClient* audio_capture_client;
	
	hr = audio_client->GetService( __uuidof(IAudioCaptureClient), (void**)&audio_capture_client );

	if ( FAILED(hr) ) 
	{
		printf("IAudioClient::GetService(IAudioCaptureClient) failed: hr 0x%08x\n", hr);
		//return;
		throw 1; //TODO:
	}

	return audio_capture_client;
}

HANDLE register_with_MMCSS()
{
	//HRESULT hr;
	DWORD nTaskIndex = 0;
	HANDLE task_handle = AvSetMmThreadCharacteristics(L"Capture", &nTaskIndex);

	//if (task_handle == NULL )		//TODO: reemplazar en todos lados NULL y 0 por nullptr
	if (task_handle == nullptr ) 
	{
		DWORD dwErr = GetLastError();
		printf("AvSetMmThreadCharacteristics failed: last error = %u\n", dwErr);
		//return HRESULT_FROM_WIN32(dwErr);
		//return;
		throw 1; //TODO:
	}

	return task_handle;
}


uint32_t get_next_packet_size( IAudioCaptureClient* audio_capture_client )
{
	HRESULT hr;


	uint32_t next_packet_size;
	hr = audio_capture_client->GetNextPacketSize(&next_packet_size);
	if (FAILED(hr)) 
	{
		//printf("IAudioCaptureClient::GetNextPacketSize failed on pass %u after %u frames: hr = 0x%08x\n", passes, frames_, hr);
		printf("IAudioCaptureClient::GetNextPacketSize failed: hr = 0x%08x\n",  hr);
		//return;
		throw 1; //TODO:
	}

	return next_packet_size;
}





struct LoopbackCaptureClass			//TODO: rename
{
	IMMDevice* mm_device_;
	bool is_int16_;
	HMMIO file_handle_;
	uint32_t frames_;
	bool done_;// = false;


	LoopbackCaptureClass()
		: done_(false)
	{}

	void stop()
	{
		done_ = true;
	}



	void operator()()
	{
		HRESULT hr;

		//TODO: ver por que es necesario !!!
		hr = CoInitialize(NULL);
		if ( FAILED(hr) )
		{
			printf("CoInitialize failed: hr = 0x%08x\n", hr);
			//return 0;
			return;
		}

		do_work();

		CoUninitialize();	//TODO: usar RAII
	}



	void do_work()
	{
		HRESULT hr;

		// activate an IAudioClient
		auto audio_client = get_active_audio_client(mm_device_);
		auto audio_client_releaser = finally ([&] { 
			audio_client->Release(); 
		});

		auto default_device_period = get_default_device_period(audio_client);	//No release
		auto millic_to_wait = (LONG)default_device_period / 2 / (10 * 1000);
		

		MMCKINFO ckRIFF;
		MMCKINFO ckData;
		uint32_t block_align;
		std::tie(ckRIFF, ckData, block_align) = inner_work_1( file_handle_, audio_client, is_int16_ );	//TODO: rename function name

		frames_ = 0;


		// activate an IAudioCaptureClient
		auto audio_capture_client = get_active_audio_capture_client( audio_client );
		auto audio_capture_client_releaser = finally ([&] { 
			audio_capture_client->Release(); 
		});


		// register with MMCSS
		auto task_handle = register_with_MMCSS();
		auto thread_characteristics_releaser = finally ([&] { 
			AvRevertMmThreadCharacteristics(task_handle);
		});



		hr = audio_client->Start();
		if (FAILED(hr))
		{
			printf("IAudioClient::Start failed: hr = 0x%08x\n", hr);
			return;
		}

		auto audio_client_stoper = finally ([&] { 
			audio_client->Stop();
		});


		bool first_packet = true;
		for ( uint32_t passes = 0; !done_; ++passes ) 
		{
			//TODO: wake up win api o Sleep... es lo mismo???
			std::this_thread::sleep_for( std::chrono::milliseconds(millic_to_wait) );

			// got a "wake up" event - see if there's data

			auto next_packet_size = get_next_packet_size( audio_capture_client );
			if ( next_packet_size == 0 )
			{
				// no data yet
				continue;
			}

			// get the captured data
			BYTE *pData;
			uint32_t nNumFramesToRead;
			DWORD dwFlags;

			hr = audio_capture_client->GetBuffer(
				&pData,
				&nNumFramesToRead,
				&dwFlags,
				NULL,
				NULL
				);
			if (FAILED(hr)) 
			{
				printf("IAudioCaptureClient::GetBuffer failed on pass %u after %u frames: hr = 0x%08x\n", passes, frames_, hr);
				return;
			}

			if (first_packet && AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags) 
			{
				printf("Probably spurious glitch reported on first packet\n");
			}
			else if (AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags) 
			{
				//TODO: que hago en este caso? debo salir? o continuar?
				printf("TODO -> ERROR: Probably spurious glitch reported on NON-first packet\n");
			} 
			else if (0 != dwFlags) 
			{
				printf("IAudioCaptureClient::GetBuffer set flags to 0x%08x on pass %u after %u frames\n", dwFlags, passes, frames_);
				return;
			}

			if (0 == nNumFramesToRead) 
			{
				printf("IAudioCaptureClient::GetBuffer said to read 0 frames on pass %u after %u frames\n", passes, frames_);
				return;
			}

			LONG lBytesToWrite = nNumFramesToRead * block_align;
#pragma prefast(suppress: __WARNING_INCORRECT_ANNOTATION, "IAudioCaptureClient::GetBuffer SAL annotation implies a 1-byte buffer")
			LONG lBytesWritten = mmioWrite(file_handle_, reinterpret_cast<PCHAR>(pData), lBytesToWrite);
			if (lBytesToWrite != lBytesWritten) 
			{
				printf("mmioWrite wrote %u bytes on pass %u after %u frames: expected %u bytes\n", lBytesWritten, passes, frames_, lBytesToWrite);
				return;
			}
			frames_ += nNumFramesToRead;

			hr = audio_capture_client->ReleaseBuffer(nNumFramesToRead);
			if (FAILED(hr)) 
			{
				printf("IAudioCaptureClient::ReleaseBuffer failed on pass %u after %u frames: hr = 0x%08x\n", passes, frames_, hr);
				return;
			}

			first_packet = false;
		} // capture loop

		//hr = finish_wave_file(file_handle_, &ckData, &ckRIFF);
		//if (FAILED(hr)) 
		//{
		//	return;
		//}
		finish_wave_file(file_handle_, &ckData, &ckRIFF);
		



		//Release order
		//audio_client->Stop();
		//AvRevertMmThreadCharacteristics(task_handle);
		//audio_capture_client->Release();
		//audio_client->Release();

		//return hr;
		return;
	}

};

#endif // LOOPBACK_CAPTURE_HPP__
