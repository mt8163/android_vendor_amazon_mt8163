#ifndef __PQALGORITHMADAPTOR_H__
#define __PQALGORITHMADAPTOR_H__

#include "PQMutex.h"
//#include "DpTileEngine.h"
//#include "DpEngineType.h"
#include "PQSession.h"
#include "PQTuningBuffer.h"

class PQTuningBuffer;
class PQAlgorithmAdaptor
{
public:
    PQAlgorithmAdaptor(ProxyTuningBuffer swreg, ProxyTuningBuffer input, ProxyTuningBuffer output);
    ~PQAlgorithmAdaptor();

protected:
    PQTuningBuffer *m_inputBuffer;
    PQTuningBuffer *m_outputBuffer;
    PQTuningBuffer *m_swRegBuffer;
};
#endif //__PQALGORITHMADAPTOR_H__