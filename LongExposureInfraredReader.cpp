#include "stdafx.h"
#include "LongExposureInfraredReader.h"

#define InfraredSourceValueMaximum static_cast<float>(USHRT_MAX)
#define InfraredOutputValueMinimum 0.01f
#define InfraredOutputValueMaximum 1.0f
#define InfraredSceneValueAverage 0.08f
#define InfraredSceneStandardDeviations 3.0f

CLongExposureInfraredReader::CLongExposureInfraredReader()
: m_pLongExposureInfraredFrameReader(nullptr)
{
}

CLongExposureInfraredReader::~CLongExposureInfraredReader()
{
	if (m_pLongExposureInfraredFrameReader && m_hFrameArrived)
	{
		m_pLongExposureInfraredFrameReader->UnsubscribeFrameArrived(m_hFrameArrived);
		m_hFrameArrived = 0;
	}

	SafeRelease(m_pLongExposureInfraredFrameReader);
}

HRESULT CLongExposureInfraredReader::Init(IKinectSensor* pKinectSensor)
{
	HRESULT	hr = E_FAIL;
	ILongExposureInfraredFrameSource* pLongExposureInfraredFrameSource = nullptr;

	hr = pKinectSensor->get_LongExposureInfraredFrameSource(&pLongExposureInfraredFrameSource);

	if (SUCCEEDED(hr))
	{
		hr = pLongExposureInfraredFrameSource->OpenReader(&m_pLongExposureInfraredFrameReader);
	}

	if (SUCCEEDED(hr) && m_ReaderMode == Event)
	{
		hr = m_pLongExposureInfraredFrameReader->SubscribeFrameArrived(&m_hFrameArrived);
	}

	SafeRelease(pLongExposureInfraredFrameSource);

	return hr;
}

HRESULT CLongExposureInfraredReader::Update()
{
	HRESULT hr = E_FAIL;
	ILongExposureInfraredFrame* pLongExposureInfraredFrame = nullptr;

	if (!m_pLongExposureInfraredFrameReader) { return hr; }

	if (m_ReaderMode == Event)
	{
		ILongExposureInfraredFrameReference* pLongExposureInfraredFrameReference = nullptr;
		ILongExposureInfraredFrameArrivedEventArgs* pLongExposureInfraredFrameArrivedEventArgs = nullptr;

		hr = m_pLongExposureInfraredFrameReader->GetFrameArrivedEventData(m_hFrameArrived, &pLongExposureInfraredFrameArrivedEventArgs);

		if (SUCCEEDED(hr))
		{
			hr = pLongExposureInfraredFrameArrivedEventArgs->get_FrameReference(&pLongExposureInfraredFrameReference);
		}

		if (SUCCEEDED(hr))
		{
			hr = pLongExposureInfraredFrameReference->AcquireFrame(&pLongExposureInfraredFrame);
		}

		SafeRelease(pLongExposureInfraredFrameReference);
		SafeRelease(pLongExposureInfraredFrameArrivedEventArgs);
	}
	else
	{
		hr = m_pLongExposureInfraredFrameReader->AcquireLatestFrame(&pLongExposureInfraredFrame);
	}

	if (SUCCEEDED(hr))
	{
		hr = Process(pLongExposureInfraredFrame);
	}

	SafeRelease(pLongExposureInfraredFrame);
	return hr;
}

HRESULT CLongExposureInfraredReader::Process(ILongExposureInfraredFrame* pLongExposureInfraredFrame)
{
	INT64 nTime = 0;
	IFrameDescription* pFrameDescription = NULL;
	int nWidth = 0;
	int nHeight = 0;
	UINT nBufferSize = 0;
	UINT16 *pBuffer = NULL;

	HRESULT hr = E_FAIL;
	if (!pLongExposureInfraredFrame) { return hr; }
	hr = pLongExposureInfraredFrame->get_RelativeTime(&nTime);

	if (SUCCEEDED(hr))
	{
		hr = pLongExposureInfraredFrame->get_FrameDescription(&pFrameDescription);
	}

	if (SUCCEEDED(hr))
	{
		hr = pFrameDescription->get_Width(&nWidth);
	}

	if (SUCCEEDED(hr))
	{
		hr = pFrameDescription->get_Height(&nHeight);
	}

	if (SUCCEEDED(hr))
	{
		hr = pLongExposureInfraredFrame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);
	}

	if (SUCCEEDED(hr))
	{
		if (m_sKinectData.m_pInfraredRGBX && pBuffer && (nWidth == cDepthWidth) && (nHeight == cDepthHeight))
		{
			RGBQUAD* pDest = reinterpret_cast<RGBQUAD*>(m_sKinectData.m_pLongExposureInfraredRGBX);

			// end pixel is start + width*height -1
			const UINT16* pBufferEnd = pBuffer + (nWidth * nHeight);

			while (pBuffer < pBufferEnd)
			{
				// normalize the incoming infrared data (ushort) to a float ranging from
				// [InfraredOutputValueMinimum, InfraredOutputValueMaximum] by
				// 1. dividing the incoming value by the source maximum value
				float intensityRatio = static_cast<float>(*pBuffer) / InfraredSourceValueMaximum;

				// 2. dividing by the (average scene value * standard deviations)
				intensityRatio /= InfraredSceneValueAverage * InfraredSceneStandardDeviations;

				// 3. limiting the value to InfraredOutputValueMaximum
				intensityRatio = min(InfraredOutputValueMaximum, intensityRatio);

				// 4. limiting the lower value InfraredOutputValueMinium
				intensityRatio = max(InfraredOutputValueMinimum, intensityRatio);

				// 5. converting the normalized value to a byte and using the result
				// as the RGB components required by the image
				byte intensity = static_cast<byte>(intensityRatio * 255.0f);
				pDest->rgbRed = intensity;
				pDest->rgbGreen = intensity;
				pDest->rgbBlue = intensity;

				++pDest;
				++pBuffer;
			}
		}
	}

	SafeRelease(pFrameDescription);

	return hr;
}