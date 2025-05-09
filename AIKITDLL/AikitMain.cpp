#include "pch.h"
#include "Common.h"
#include "IvwWrapper.h"
#include <string.h>
#include <aikit_constant.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif
	AIKITDLL_API int RunFullTest()
	{
		FILE* fin = nullptr;
		/**
		 * ������Ȩ������������������ö��������������";"�ָ���, ��"e867a88f2;ece9d3c90"
		 * �������� id = e867a88f2
		 * ���������ʶ�� id = e75f07b62
		 * ���������ϳ� id = e2e44feff
		*/
		// ���������Ҫʹ�õ�����ID
		const char* ability_id = "e867a88f2;e75f07b62";  // ��������ϳ�����ID
		if (strlen(ability_id) == 0)
		{
			printf("ability_id is empty!!\n");
			AIKITDLL::lastResult = "ERROR: Ability ID is empty. Please provide valid ability IDs.";
			return -1;
		}
		// ������ϸ����־���
		AIKITDLL::LogDebug("��ʼ��ʼ��AIKIT SDK��������������...\n");

		// ����ؼ�����
		const char* appID = "80857b22";
		const char* apiSecret = "ZDkyM2RkZjcyYTZmM2VjMDM0MzVmZDJl";
		const char* apiKey = "f27d0261fb5fa1733b7e9a2190006aec";
		const char* workDir = ".";
		const char* abilityID = IVW_ABILITY; // ʹ�ö���õĳ���

		// �������ò����SDK��ʼ��
		auto config = AIKIT::AIKIT_Configurator::builder()
			.app()
			.appID(appID)
			.apiSecret(apiSecret)
			.apiKey(apiKey)
			.workDir(workDir)
			.auth()
			.authType(0)
			.ability(ability_id)
			.log()
			.logLevel(LOG_LVL_INFO)
			.logMode(2)
			.logPath("D:\\AIKITDLL\\aikit.log");

		int ret = AIKIT::AIKIT_Init();
		if (ret != 0) {
			printf("AIKIT_Init failed, error code: %d\n", ret);
			AIKITDLL::lastResult = "ERROR: AIKIT initialization failed with code: " + std::to_string(ret);
			return -1;
		}

		AIKITDLL::LogDebug("AIKIT SDK��ʼ���ɹ�����ʼ��ʼ��������������...\n");
		AIKITDLL::lastResult = "INFO: AIKIT��ʼ���ɹ���";

		// ���ûص�����
		AIKIT_Callbacks cbs = { AIKITDLL::OnOutput, AIKITDLL::OnEvent, AIKITDLL::OnError };

		AIKITDLL::LogDebug("����Ivw70Init");
		ret = Ivw70Init();
		if (ret != 0) {
			AIKIT::AIKIT_UnInit();
			return -1;
		}
		AIKITDLL::LogDebug("INFO: Voice wake-up ��ʼ���ɹ�");
		AIKITDLL::LogDebug("����TestIvw70");
		int ivwResult = 0;
		do {
			AIKITDLL::LogDebug("����TestIvw70");
			ivwResult = TestIvw70(cbs);
			if (ivwResult != 0) {
				AIKITDLL::LogWarning("TestIvw70 failed with code: %d, retrying...", ivwResult);
				// ��ѡ��: �ȴ�һ��ʱ��������
				Sleep(1000);  // �ȴ�1��������
			}
		} while (ivwResult != 0);

		AIKITDLL::LogInfo("TestIvw70 succeeded, proceeding to TestEsr");

		AIKITDLL::LogDebug("����TestEsr");
		TestEsr(cbs);

		// ȷ���ر��ļ���Դ
		if (fin != nullptr) {
			fclose(fin);
			fin = nullptr;
		}

		// �����˳�����
		Ivw70Uninit();      //��Ҫ��Ivw70Init�ɶԳ���
		AIKITDLL::lastResult = "INFO: ��������ģ����ж�ء�";
		AIKIT::AIKIT_UnInit();
		AIKITDLL::lastResult = "SUCCESS: ���Գɹ���ɡ�������Դ���ͷš�";
		return 0;
	}
#ifdef __cplusplus
}
#endif