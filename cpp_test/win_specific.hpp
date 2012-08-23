#ifndef win_specific_HPP__
#define win_specific_HPP__


struct com_initializer
{
	com_initializer()
		: initialized_(false)
	{
		HRESULT hr = S_OK;
		hr = CoInitialize(NULL);		//TODO: CoInitializeEx ??
		if ( FAILED(hr) )
		{
			//TODO:
			printf("CoInitialize failed: hr = 0x%08x", hr);
			//return -__LINE__;
		}

		initialized_ = true;
	}

	~com_initializer()
	{
		CoUninitialize();
	}


	bool initialized_;// = false;		//Not supported in MSVC-11
};

#endif // win_specific_HPP__
