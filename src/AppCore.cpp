#include "stdafx.h"
#include "TVTest.h"
#include "AppCore.h"
#include "AppMain.h"
#include "AppUtil.h"
#include "CardReaderDialog.h"
#include "resource.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


using namespace TVTest;




CAppCore::CAppCore(CAppMain &App)
	: m_App(App)
	, m_fSilent(false)
	, m_fExitOnRecordingStop(false)
	, m_fCasCardOpenError(false)
	, m_f1SegMode(false)
{
	::GetModuleFileName(nullptr,m_szChannelSettingFileName,MAX_PATH);
	::PathRenameExtension(m_szChannelSettingFileName,TEXT(".ch.ini"));
}


bool CAppCore::GetDriverDirectory(LPTSTR pszDirectory,int MaxLength) const
{
	return m_App.CoreEngine.GetDriverDirectory(pszDirectory,MaxLength);
}


void CAppCore::AddLog(LPCTSTR pszText, ...)
{
	va_list Args;

	va_start(Args,pszText);
	m_App.Logger.AddLogV(pszText,Args);
	va_end(Args);
}


void CAppCore::OnError(const CBonErrorHandler *pErrorHandler,LPCTSTR pszTitle)
{
	if (pErrorHandler==nullptr)
		return;
	m_App.Logger.AddLog(TEXT("%s"),pErrorHandler->GetLastErrorText());
	if (!m_fSilent)
		m_App.UICore.GetSkin()->ShowErrorMessage(pErrorHandler,pszTitle);
}


void CAppCore::SetSilent(bool fSilent)
{
	m_fSilent=fSilent;
#ifndef _DEBUG
	m_App.DebugHelper.SetExceptionFilterMode(
		fSilent?CDebugHelper::EXCEPTION_FILTER_NONE:CDebugHelper::EXCEPTION_FILTER_DIALOG);
#endif
}


bool CAppCore::InitializeChannel()
{
	const bool fNetworkDriver=m_App.CoreEngine.IsNetworkDriver();
	CFilePath ChannelFilePath;
#ifdef NETWORK_REMOCON_SUPPORT
	TCHAR szNetworkDriverName[MAX_PATH];
#endif

	m_App.ChannelManager.Reset();
	m_App.ChannelManager.MakeDriverTuningSpaceList(&m_App.CoreEngine.m_DtvEngine.m_BonSrcDecoder);

	if (!fNetworkDriver) {
		TCHAR szPath[MAX_PATH];
		GetChannelFileName(m_App.CoreEngine.GetDriverFileName(),szPath,MAX_PATH);
		ChannelFilePath.SetPath(szPath);
	}
#ifdef NETWORK_REMOCON_SUPPORT
	else {
		bool fOK=false;

		if (m_App.NetworkRemoconOptions.IsEnable()) {
			if (m_App.NetworkRemoconOptions.CreateNetworkRemocon(&m_App.pNetworkRemocon)) {
				m_App.NetworkRemoconGetDriver.Initialize();
				if (m_App.pNetworkRemocon->GetDriverList(&m_App.NetworkRemoconGetDriver)
						&& m_App.NetworkRemoconGetDriver.Wait(2000)
						&& m_App.NetworkRemoconGetDriver.GetCurDriver()[0]!=_T('\0')) {
					TCHAR szFileName[MAX_PATH];

					if (m_App.NetworkRemoconOptions.FindChannelFile(
							m_App.NetworkRemoconGetDriver.GetCurDriver(),szFileName)) {
						LPTSTR p;

						m_App.NetworkRemoconOptions.SetDefaultChannelFileName(szFileName);
						p=szFileName;
						while (*p!=_T('('))
							p++;
						*p=_T('\0');
						::wsprintf(szNetworkDriverName,TEXT("%s.dll"),szFileName);
						::lstrcpy(p,CHANNEL_FILE_EXTENSION);
						ChannelFilePath.SetPath(szFileName);
						m_App.GetAppDirectory(szFileName);
						ChannelFilePath.SetDirectory(szFileName);
						fOK=ChannelFilePath.IsExists();
#ifdef DEFERRED_CHANNEL_FILE_EXTENSION
						if (!fOK) {
							ChannelFilePath.SetExtension(DEFERRED_CHANNEL_FILE_EXTENSION);
							fOK=ChannelFilePath.IsExists();
						}
#endif
					}
				}
			}
			if (!fOK && !IsStringEmpty(m_App.NetworkRemoconOptions.GetChannelFileName())) {
				TCHAR szFileName[MAX_PATH],*q;
				LPCTSTR p;

				m_App.GetAppDirectory(szFileName);
				ChannelFilePath.SetPath(szFileName);
				p=m_App.NetworkRemoconOptions.GetChannelFileName();
				q=szFileName;
				while (*p!=_T('(') && *p!=_T('\0')) {
					int Length=StringCharLength(p);
					if (Length==0)
						break;
					for (int i=0;i<Length;i++)
						*q++=*p++;
				}
				*q=_T('\0');
				::wsprintf(szNetworkDriverName,TEXT("%s.dll"),szFileName);
				::lstrcpy(q,CHANNEL_FILE_EXTENSION);
				ChannelFilePath.Append(szFileName);
				fOK=ChannelFilePath.IsExists();
#ifdef DEFERRED_CHANNEL_FILE_EXTENSION
				if (!fOK) {
					ChannelFilePath.SetExtension(DEFERRED_CHANNEL_FILE_EXTENSION);
					fOK=ChannelFilePath.IsExists();
				}
#endif
			}
		}
		if (!fOK)
			szNetworkDriverName[0]=_T('\0');
	}
#endif	// NETWORK_REMOCON_SUPPORT

	if (!ChannelFilePath.IsEmpty()) {
		if (m_App.ChannelManager.LoadChannelList(ChannelFilePath.GetPath())) {
			AddLog(TEXT("�`�����l���ݒ�� \"%s\" ����ǂݍ��݂܂����B"),
				   ChannelFilePath.GetPath());
			if (!m_App.ChannelManager.ChannelFileHasStreamIDs())
				AddLog(TEXT("(�`�����l���t�@�C�����Â��̂ōăX�L���������E�߂��܂�)"));
		}
	}

	TCHAR szFileName[MAX_PATH];
	bool fLoadChannelSettings=true;
	if (!fNetworkDriver) {
		::lstrcpy(szFileName,m_App.CoreEngine.GetDriverFileName());
	} else {
#ifdef NETWORK_REMOCON_SUPPORT
		if (szNetworkDriverName[0]!=_T('\0')) {
			::lstrcpy(szFileName,szNetworkDriverName);
		} else
#endif
		{
			fLoadChannelSettings=false;
		}
	}
	if (fLoadChannelSettings)
		m_App.ChannelManager.LoadChannelSettings(m_szChannelSettingFileName,szFileName);

	CDriverOptions::ChannelInfo InitChInfo;
	if (m_App.DriverOptions.GetInitialChannel(m_App.CoreEngine.GetDriverFileName(),&InitChInfo)) {
		m_App.RestoreChannelInfo.Space=InitChInfo.Space;
		m_App.RestoreChannelInfo.Channel=InitChInfo.Channel;
		m_App.RestoreChannelInfo.ServiceID=InitChInfo.ServiceID;
		m_App.RestoreChannelInfo.TransportStreamID=InitChInfo.TransportStreamID;
		m_App.RestoreChannelInfo.fAllChannels=InitChInfo.fAllChannels;
	} else {
		m_App.RestoreChannelInfo.Space=-1;
		m_App.RestoreChannelInfo.Channel=-1;
		m_App.RestoreChannelInfo.ServiceID=-1;
		m_App.RestoreChannelInfo.TransportStreamID=-1;
		m_App.RestoreChannelInfo.fAllChannels=false;
	}

	m_App.ChannelManager.SetUseDriverChannelList(fNetworkDriver);
	m_App.ChannelManager.SetCurrentChannel(
		m_App.RestoreChannelInfo.fAllChannels?CChannelManager::SPACE_ALL:max(m_App.RestoreChannelInfo.Space,0),
		-1);
	m_App.ChannelManager.SetCurrentServiceID(0);
#ifdef NETWORK_REMOCON_SUPPORT
	m_App.NetworkRemoconOptions.InitNetworkRemocon(&m_App.pNetworkRemocon,
											 &m_App.CoreEngine,&m_App.ChannelManager);
#endif
	m_App.UICore.OnChannelListChanged();
	m_App.ChannelScan.SetTuningSpaceList(m_App.ChannelManager.GetTuningSpaceList());
	return true;
}


bool CAppCore::GetChannelFileName(LPCTSTR pszDriverFileName,
								  LPTSTR pszChannelFileName,int MaxChannelFileName)
{
	if (IsStringEmpty(pszDriverFileName) || pszChannelFileName==nullptr)
		return false;

	const bool fRelative=::PathIsRelative(pszDriverFileName)!=FALSE;
	TCHAR szPath[MAX_PATH],szPath2[MAX_PATH],szDir[MAX_PATH];
	if (fRelative) {
		if (!m_App.CoreEngine.GetDriverDirectory(szDir,lengthof(szDir)))
			return false;
		if (::PathCombine(szPath,szDir,pszDriverFileName)==nullptr)
			return false;
	} else {
		if (::lstrlen(pszDriverFileName)>=lengthof(szPath))
			return false;
		::lstrcpy(szPath,pszDriverFileName);
	}
	::PathRenameExtension(szPath,CHANNEL_FILE_EXTENSION);
#ifdef DEFERRED_CHANNEL_FILE_EXTENSION
	if (!::PathFileExists(szPath)) {
		::lstrcpy(szPath2,szPath);
		::PathRenameExtension(szPath2,DEFERRED_CHANNEL_FILE_EXTENSION);
		if (::PathFileExists(szPath2))
			::lstrcpy(szPath,szPath2);
	}
#endif
	if (fRelative && !::PathFileExists(szPath)) {
		m_App.GetAppDirectory(szDir);
		if (::PathCombine(szPath2,szDir,pszDriverFileName)!=nullptr) {
			::PathRenameExtension(szPath2,CHANNEL_FILE_EXTENSION);
			if (::PathFileExists(szPath2)) {
				::lstrcpy(szPath,szPath2);
			}
#ifdef DEFERRED_CHANNEL_FILE_EXTENSION
			else {
				::PathRenameExtension(szPath2,DEFERRED_CHANNEL_FILE_EXTENSION);
				if (::PathFileExists(szPath2))
					::lstrcpy(szPath,szPath2);
			}
#endif
		}
	}
	if (::lstrlen(szPath)>=MaxChannelFileName)
		return false;
	::lstrcpy(pszChannelFileName,szPath);
	return true;
}


bool CAppCore::RestoreChannel()
{
	if (m_App.RestoreChannelInfo.Space>=0 && m_App.RestoreChannelInfo.Channel>=0) {
		int Space=m_App.RestoreChannelInfo.fAllChannels?CChannelManager::SPACE_ALL:m_App.RestoreChannelInfo.Space;
		const CChannelList *pList=m_App.ChannelManager.GetChannelList(Space);
		if (pList!=nullptr) {
			int Index=pList->FindByIndex(m_App.RestoreChannelInfo.Space,
										 m_App.RestoreChannelInfo.Channel,
										 m_App.RestoreChannelInfo.ServiceID);
			if (Index<0) {
				if (m_App.RestoreChannelInfo.TransportStreamID>0 && m_App.RestoreChannelInfo.ServiceID>0) {
					Index=pList->FindByIDs(0,
										   (WORD)m_App.RestoreChannelInfo.TransportStreamID,
										   (WORD)m_App.RestoreChannelInfo.ServiceID);
				}
				if (Index<0 && m_App.RestoreChannelInfo.ServiceID>0) {
					Index=pList->FindByIndex(m_App.RestoreChannelInfo.Space,
											 m_App.RestoreChannelInfo.Channel);
				}
			}
			if (Index>=0)
				return SetChannel(Space,Index);
		}
	}
	return false;
}


bool CAppCore::UpdateCurrentChannelList(const CTuningSpaceList *pList)
{
	bool fNetworkDriver=m_App.CoreEngine.IsNetworkDriver();

	m_App.ChannelManager.SetTuningSpaceList(pList);
	m_App.ChannelManager.SetUseDriverChannelList(fNetworkDriver);
	/*
	m_App.ChannelManager.SetCurrentChannel(
		(!fNetworkDriver && m_App.ChannelManager.GetAllChannelList()->NumChannels()>0)?
			CChannelManager::SPACE_ALL:0,
		m_App.CoreEngine.IsUDPDriver()?0:-1);
	*/
	int Space=-1;
	bool fAllChannels=false;
	for (int i=0;i<pList->NumSpaces();i++) {
		if (pList->GetTuningSpaceType(i)!=CTuningSpaceInfo::SPACE_TERRESTRIAL) {
			fAllChannels=false;
			break;
		}
		if (pList->GetChannelList(i)->NumChannels()>0) {
			if (Space>=0)
				fAllChannels=true;
			else
				Space=i;
		}
	}
	m_App.ChannelManager.SetCurrentChannel(
		fAllChannels?CChannelManager::SPACE_ALL:(Space>=0?Space:0),
		-1);
	m_App.ChannelManager.SetCurrentServiceID(0);
	WORD ServiceID;
	if (m_App.CoreEngine.m_DtvEngine.GetServiceID(&ServiceID))
		FollowChannelChange(m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetTransportStreamID(),ServiceID);
#ifdef NETWORK_REMOCON_SUPPORT
	m_App.NetworkRemoconOptions.InitNetworkRemocon(&m_App.pNetworkRemocon,
												   &m_App.CoreEngine,&m_App.ChannelManager);
#endif

	m_App.UICore.OnChannelListChanged();

	UpdateChannelList(m_App.CoreEngine.GetDriverFileName(),pList);

	return true;
}


bool CAppCore::UpdateChannelList(LPCTSTR pszBonDriverName,const CTuningSpaceList *pList)
{
	if (IsStringEmpty(pszBonDriverName) || pList==nullptr)
		return false;

	int Index=m_App.DriverManager.FindByFileName(::PathFindFileName(pszBonDriverName));
	if (Index>=0) {
		CDriverInfo *pDriverInfo=m_App.DriverManager.GetDriverInfo(Index);
		if (pDriverInfo!=nullptr) {
			pDriverInfo->ClearTuningSpaceList();
		}
	}

	// ���C�ɓ���`�����l���̍X�V
	class CFavoritesChannelUpdator : public CFavoriteItemEnumerator
	{
		LPCTSTR m_pszBonDriver;
		const CTuningSpaceList *m_pTuningSpaceList;
		bool m_fUpdated;

		bool ChannelItem(CFavoriteFolder &Folder,CFavoriteChannel &Channel) override
		{
			if (IsEqualFileName(Channel.GetBonDriverFileName(),m_pszBonDriver)) {
				const CChannelInfo &ChannelInfo=Channel.GetChannelInfo();
				const CChannelList *pChannelList=m_pTuningSpaceList->GetChannelList(ChannelInfo.GetSpace());

				if (pChannelList!=nullptr) {
					if (pChannelList->FindByIDs(
							ChannelInfo.GetNetworkID(),
							ChannelInfo.GetTransportStreamID(),
							ChannelInfo.GetServiceID(),
							false)<0) {
						const int ChannelCount=pChannelList->NumChannels();
						const CNetworkDefinition &NetworkDefinition=GetAppClass().NetworkDefinition;
						const CNetworkDefinition::NetworkType ChannelNetworkType=
							NetworkDefinition.GetNetworkType(ChannelInfo.GetNetworkID());

						for (int i=0;i<ChannelCount;i++) {
							const CChannelInfo *pChInfo=pChannelList->GetChannelInfo(i);

							if (NetworkDefinition.GetNetworkType(pChInfo->GetNetworkID())==ChannelNetworkType
									&& (pChInfo->GetServiceID()==ChannelInfo.GetServiceID()
										|| ::lstrcmp(pChInfo->GetName(),ChannelInfo.GetName())==0)) {
								TRACE(TEXT("���C�ɓ���`�����l���X�V : %s -> %s / NID %d -> %d / TSID %04x -> %04x / SID %d -> %d\n"),
									  ChannelInfo.GetName(),pChInfo->GetName(),
									  ChannelInfo.GetNetworkID(),pChInfo->GetNetworkID(),
									  ChannelInfo.GetTransportStreamID(),pChInfo->GetTransportStreamID(),
									  ChannelInfo.GetServiceID(),pChInfo->GetServiceID());
								Channel.SetChannelInfo(*pChInfo);
								m_fUpdated=true;
								break;
							}
						}
					}
				}
			}

			return true;
		}

	public:
		CFavoritesChannelUpdator(LPCTSTR pszBonDriver,const CTuningSpaceList *pTuningSpaceList)
			: m_pszBonDriver(pszBonDriver)
			, m_pTuningSpaceList(pTuningSpaceList)
			, m_fUpdated(false)
		{
		}

		bool IsUpdated() const { return m_fUpdated; }
	};

	CFavoritesChannelUpdator FavoritesUpdator(pszBonDriverName,pList);
	FavoritesUpdator.EnumItems(m_App.FavoritesManager.GetRootFolder());
	if (FavoritesUpdator.IsUpdated())
		m_App.FavoritesManager.SetModified(true);

	return true;
}


bool CAppCore::SaveChannelSettings()
{
	if (!m_App.CoreEngine.IsTunerOpen() || m_App.CoreEngine.IsNetworkDriver())
		return true;
	return m_App.ChannelManager.SaveChannelSettings(m_szChannelSettingFileName,
													m_App.CoreEngine.GetDriverFileName());
}


const CChannelInfo *CAppCore::GetCurrentChannelInfo() const
{
	return m_App.ChannelManager.GetCurrentChannelInfo();
}


bool CAppCore::SetChannel(int Space,int Channel,int ServiceID/*=-1*/,bool fStrictService/*=false*/)
{
	const CChannelInfo *pPrevChInfo=m_App.ChannelManager.GetCurrentRealChannelInfo();
	int OldSpace=m_App.ChannelManager.GetCurrentSpace(),OldChannel=m_App.ChannelManager.GetCurrentChannel();

	if (!m_App.ChannelManager.SetCurrentChannel(Space,Channel))
		return false;
	const CChannelInfo *pChInfo=m_App.ChannelManager.GetCurrentRealChannelInfo();
	if (pChInfo==nullptr) {
		m_App.ChannelManager.SetCurrentChannel(OldSpace,OldChannel);
		return false;
	}
	if (pPrevChInfo==nullptr
			|| pChInfo->GetSpace()!=pPrevChInfo->GetSpace()
			|| pChInfo->GetChannelIndex()!=pPrevChInfo->GetChannelIndex()) {
		if (ServiceID>0) {
			const CChannelList *pChList=m_App.ChannelManager.GetCurrentRealChannelList();
			int Index=pChList->FindByIndex(pChInfo->GetSpace(),
										   pChInfo->GetChannelIndex(),
										   ServiceID);
			if (Index>=0) {
				m_App.ChannelManager.SetCurrentChannel(Space,Index);
				pChInfo=pChList->GetChannelInfo(Index);
			}
		} else {
			if (pChInfo->GetServiceID()>0)
				ServiceID=pChInfo->GetServiceID();
		}

		LPCTSTR pszTuningSpace=m_App.ChannelManager.GetDriverTuningSpaceList()->GetTuningSpaceName(pChInfo->GetSpace());
		AddLog(TEXT("BonDriver�Ƀ`�����l���ύX��v�����܂��B(�`���[�j���O��� %d[%s] / Ch %d[%s] / Sv %d)"),
			   pChInfo->GetSpace(),pszTuningSpace!=nullptr?pszTuningSpace:TEXT("\?\?\?"),
			   pChInfo->GetChannelIndex(),pChInfo->GetName(),ServiceID);

		CDtvEngine::ServiceSelectInfo ServiceSel;
		ServiceSel.ServiceID=ServiceID>0?ServiceID:CDtvEngine::SID_INVALID;
		ServiceSel.bFollowViewableService=!m_App.NetworkDefinition.IsCSNetworkID(pChInfo->GetNetworkID());

		if (!fStrictService && m_f1SegMode) {
			ServiceSel.bPrefer1Seg=true;

			// �T�u�`�����l���̑I���̏ꍇ�A�����Z�O���T�u�`�����l����D�悷��
			if (ServiceSel.ServiceID!=CDtvEngine::SID_INVALID) {
				ServiceSel.PreferredServiceIndex=
					(WORD)GetCorresponding1SegService(
						pChInfo->GetSpace(),
						pChInfo->GetNetworkID(),
						pChInfo->GetTransportStreamID(),
						ServiceSel.ServiceID);
			}
		}

		if (!m_App.CoreEngine.m_DtvEngine.SetChannel(
				pChInfo->GetSpace(),pChInfo->GetChannelIndex(),&ServiceSel)) {
			AddLog(TEXT("%s"),m_App.CoreEngine.m_DtvEngine.GetLastErrorText());
			m_App.ChannelManager.SetCurrentChannel(OldSpace,OldChannel);
			return false;
		}

		m_App.ChannelManager.SetCurrentServiceID(ServiceID);
		m_App.UICore.OnChannelChanged(Space!=OldSpace ? CUICore::CHANNEL_CHANGED_STATUS_SPACE_CHANGED : 0);
		m_App.PluginManager.SendChannelChangeEvent();
	} else {
		if (ServiceID<=0) {
			if (pChInfo->GetServiceID()>0)
				ServiceID=pChInfo->GetServiceID();
			else
				return false;
		}
		if (!SetServiceByID(ServiceID,fStrictService?SET_SERVICE_STRICT_ID:0))
			return false;
	}

	return true;
}


bool CAppCore::SetChannelByIndex(int Space,int Channel,int ServiceID)
{
	const CChannelList *pChannelList=m_App.ChannelManager.GetChannelList(Space);
	if (pChannelList==nullptr)
		return false;

	int ListChannel=pChannelList->FindByIndex(Space,Channel,ServiceID);
	if (ListChannel<0) {
		if (ServiceID>0)
			ListChannel=pChannelList->FindByIndex(Space,Channel);
		if (ListChannel<0)
			return false;
	}

	return SetChannel(Space,ListChannel,ServiceID);
}


bool CAppCore::SelectChannel(const ChannelSelectInfo &SelInfo)
{
	if (SelInfo.fUseCurTuner
			&& m_App.CoreEngine.IsTunerOpen()) {
		int Space=m_App.ChannelManager.GetCurrentSpace();
		if (Space!=CChannelManager::SPACE_INVALID) {
			int Index=m_App.ChannelManager.FindChannelByIDs(Space,
				SelInfo.Channel.GetNetworkID(),
				SelInfo.Channel.GetTransportStreamID(),
				SelInfo.Channel.GetServiceID());
			if (Index<0 && Space!=CChannelManager::SPACE_ALL) {
				for (Space=0;Space<m_App.ChannelManager.NumSpaces();Space++) {
					Index=m_App.ChannelManager.FindChannelByIDs(Space,
						SelInfo.Channel.GetNetworkID(),
						SelInfo.Channel.GetTransportStreamID(),
						SelInfo.Channel.GetServiceID());
					if (Index>=0)
						break;
				}
			}
			if (Index>=0) {
				if (!m_App.UICore.ConfirmChannelChange())
					return false;
				return SetChannel(Space,Index);
			}
		}
	} else if (SelInfo.TunerName.empty()) {
		return false;
	}

	return OpenTunerAndSetChannel(SelInfo.TunerName.c_str(),&SelInfo.Channel);
}


bool CAppCore::SwitchChannel(int Channel)
{
	const CChannelList *pChList=m_App.ChannelManager.GetCurrentRealChannelList();
	if (pChList==nullptr)
		return false;
	const CChannelInfo *pChInfo=pChList->GetChannelInfo(Channel);
	if (pChInfo==nullptr)
		return false;

	if (!m_App.UICore.ConfirmChannelChange())
		return false;

	return SetChannel(m_App.ChannelManager.GetCurrentSpace(),Channel);
}


bool CAppCore::SwitchChannelByNo(int ChannelNo,bool fSwitchService)
{
	if (ChannelNo<1)
		return false;

#ifdef NETWORK_REMOCON_SUPPORT
	if (m_App.pNetworkRemocon!=nullptr) {
		if (!m_App.UICore.ConfirmChannelChange())
			return false;
		m_App.pNetworkRemocon->SetChannel(ChannelNo-1);
		m_App.ChannelManager.SetNetworkRemoconCurrentChannel(
			m_App.ChannelManager.GetCurrentChannelList()->FindChannelNo(ChannelNo));
		m_App.UICore.OnChannelChanged(0);
		m_App.PluginManager.SendChannelChangeEvent();
		return true;
	}
#endif

	const CChannelList *pList=m_App.ChannelManager.GetCurrentChannelList();
	if (pList==nullptr)
		return false;

	int Index;

	if (pList->HasRemoteControlKeyID()) {
		Index=pList->FindChannelNo(ChannelNo);

		if (fSwitchService) {
			const CChannelInfo *pCurChInfo=m_App.ChannelManager.GetCurrentChannelInfo();

			if (pCurChInfo!=nullptr && pCurChInfo->GetChannelNo()==ChannelNo) {
				const int NumChannels=pList->NumChannels();

				for (int i=m_App.ChannelManager.GetCurrentChannel()+1;i<NumChannels;i++) {
					const CChannelInfo *pChInfo=pList->GetChannelInfo(i);

					if (pChInfo->IsEnabled() && pChInfo->GetChannelNo()==ChannelNo) {
						Index=i;
						break;
					}
				}
			}
		}

		if (Index<0)
			return false;
	} else {
		Index=ChannelNo-1;
	}

	return SwitchChannel(Index);
}


bool CAppCore::SetCommandLineChannel(const CCommandLineOptions *pCmdLine)
{
#ifdef NETWORK_REMOCON_SUPPORT
	if (m_App.ChannelManager.IsNetworkRemoconMode()) {
		if (pCmdLine->m_ControllerChannel==0)
			return false;
		if (m_App.ChannelManager.GetCurrentChannelList()->FindChannelNo(pCmdLine->m_ControllerChannel)>=0) {
			return SwitchChannelByNo(pCmdLine->m_ControllerChannel,false);
		}
		return false;
	}
#endif

	const CChannelList *pChannelList;

	for (int i=0;(pChannelList=m_App.ChannelManager.GetChannelList(i))!=nullptr;i++) {
		if (pCmdLine->m_TuningSpace<0 || i==pCmdLine->m_TuningSpace) {
			for (int j=0;j<pChannelList->NumChannels();j++) {
				const CChannelInfo *pChannelInfo=pChannelList->GetChannelInfo(j);

				if ((pCmdLine->m_Channel==0
						|| pCmdLine->m_Channel==pChannelInfo->GetPhysicalChannel())
					&& (pCmdLine->m_ControllerChannel==0
						|| pCmdLine->m_ControllerChannel==pChannelInfo->GetChannelNo())
					&& (pCmdLine->m_ServiceID==0
						|| pCmdLine->m_ServiceID==pChannelInfo->GetServiceID())
					&& (pCmdLine->m_NetworkID==0
						|| pCmdLine->m_NetworkID==pChannelInfo->GetNetworkID())
					&& (pCmdLine->m_TransportStreamID==0
						|| pCmdLine->m_TransportStreamID==pChannelInfo->GetTransportStreamID())) {
					return SetChannel(i,j);
				}
			}
		}
	}

	if (pCmdLine->m_ServiceID>0
			&& (pCmdLine->m_Channel>0 || pCmdLine->m_ControllerChannel>0
				|| pCmdLine->m_NetworkID>0 || pCmdLine->m_TransportStreamID>0)) {
		for (int i=0;(pChannelList=m_App.ChannelManager.GetChannelList(i))!=nullptr;i++) {
			if (pCmdLine->m_TuningSpace<0 || i==pCmdLine->m_TuningSpace) {
				for (int j=0;j<pChannelList->NumChannels();j++) {
					const CChannelInfo *pChannelInfo=pChannelList->GetChannelInfo(j);

					if ((pCmdLine->m_Channel==0
							|| pCmdLine->m_Channel==pChannelInfo->GetPhysicalChannel())
						&& (pCmdLine->m_ControllerChannel==0
							|| pCmdLine->m_ControllerChannel==pChannelInfo->GetChannelNo())
						&& (pCmdLine->m_NetworkID==0
							|| pCmdLine->m_NetworkID==pChannelInfo->GetNetworkID())
						&& (pCmdLine->m_TransportStreamID==0
							|| pCmdLine->m_TransportStreamID==pChannelInfo->GetTransportStreamID())) {
						return SetChannel(i,j,pCmdLine->m_ServiceID);
					}
				}
			}
		}
	}

	AddLog(TEXT("�R�}���h���C���Ŏw�肳�ꂽ�`�����l�������t����܂���B"));

	return false;
}


bool CAppCore::FollowChannelChange(WORD TransportStreamID,WORD ServiceID)
{
	const CChannelList *pChannelList;
	const CChannelInfo *pChannelInfo=nullptr;
	int Space,Channel;

	pChannelList=m_App.ChannelManager.GetCurrentRealChannelList();
	if (pChannelList!=nullptr) {
		Channel=pChannelList->FindByIDs(0,TransportStreamID,ServiceID);
		if (Channel>=0) {
			pChannelInfo=pChannelList->GetChannelInfo(Channel);
			Space=m_App.ChannelManager.GetCurrentSpace();
		}
	} else {
		for (int i=0;i<m_App.ChannelManager.NumSpaces();i++) {
			pChannelList=m_App.ChannelManager.GetChannelList(i);
			Channel=pChannelList->FindByIDs(0,TransportStreamID,ServiceID);
			if (Channel>=0) {
				pChannelInfo=pChannelList->GetChannelInfo(Channel);
				Space=i;
				break;
			}
		}
	}
	if (pChannelInfo==nullptr)
		return false;

	const CChannelInfo *pCurChInfo=m_App.ChannelManager.GetCurrentRealChannelInfo();
	if (pCurChInfo==nullptr
			|| pCurChInfo->GetTransportStreamID()!=TransportStreamID) {
		AddLog(TEXT("�X�g���[���̕ω������m���܂����B(TSID %d / SID %d)"),
			   TransportStreamID,ServiceID);
	}
	const bool fSpaceChanged=Space!=m_App.ChannelManager.GetCurrentSpace();
	if (!m_App.ChannelManager.SetCurrentChannel(Space,Channel))
		return false;
	m_App.ChannelManager.SetCurrentServiceID(0);
	m_App.UICore.OnChannelChanged(CUICore::CHANNEL_CHANGED_STATUS_DETECTED
		| (fSpaceChanged ? CUICore::CHANNEL_CHANGED_STATUS_SPACE_CHANGED : 0));
	m_App.PluginManager.SendChannelChangeEvent();
	return true;
}


bool CAppCore::SetServiceByID(WORD ServiceID,unsigned int Flags)
{
	TRACE(TEXT("CAppCore::SetServiceByID(%04x,%x)\n"),ServiceID,Flags);

	const bool fStrict=(Flags & SET_SERVICE_STRICT_ID)!=0;
	const CChannelInfo *pCurChInfo=m_App.ChannelManager.GetCurrentRealChannelInfo();
	WORD NetworkID=m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetNetworkID();
	if (NetworkID==0 && pCurChInfo!=nullptr && pCurChInfo->GetNetworkID()>0)
		NetworkID=pCurChInfo->GetNetworkID();

	CDtvEngine::ServiceSelectInfo ServiceSel;

	ServiceSel.ServiceID=ServiceID!=0?ServiceID:CDtvEngine::SID_INVALID;
	ServiceSel.bFollowViewableService=!m_App.NetworkDefinition.IsCSNetworkID(NetworkID);

	if (!fStrict && m_f1SegMode) {
		ServiceSel.bPrefer1Seg=true;

		if (pCurChInfo!=nullptr) {
			// �T�u�`�����l���̑I���̏ꍇ�A�����Z�O���T�u�`�����l����D�悷��
			if (ServiceSel.ServiceID!=CDtvEngine::SID_INVALID) {
				ServiceSel.PreferredServiceIndex=
					(WORD)GetCorresponding1SegService(
						pCurChInfo->GetSpace(),
						pCurChInfo->GetNetworkID(),
						pCurChInfo->GetTransportStreamID(),
						ServiceSel.ServiceID);
			}
		}
	}

	bool fResult;

	if (ServiceSel.ServiceID==CDtvEngine::SID_DEFAULT) {
		AddLog(TEXT("�f�t�H���g�̃T�[�r�X��I�����܂�..."));
		fResult=m_App.CoreEngine.m_DtvEngine.SetService(&ServiceSel);
		if (fResult) {
			if (!m_App.CoreEngine.m_DtvEngine.GetServiceID(&ServiceID))
				ServiceID=0;
		}
	} else {
		if (ServiceSel.bPrefer1Seg)
			AddLog(TEXT("�T�[�r�X��I�����܂�..."));
		else
			AddLog(TEXT("�T�[�r�X��I�����܂�(SID %d)..."),ServiceSel.ServiceID);
		fResult=m_App.CoreEngine.m_DtvEngine.SetService(&ServiceSel);
	}
	if (!fResult) {
		AddLog(TEXT("�T�[�r�X��I���ł��܂���B"));
		return false;
	}

	if ((Flags & SET_SERVICE_NO_CHANGE_CUR_SERVICE_ID)==0)
		m_App.ChannelManager.SetCurrentServiceID(ServiceID);

	if (ServiceID!=0) {
		int ServiceIndex=m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetServiceIndexByID(ServiceID);
		if (ServiceIndex>=0) {
#ifdef NETWORK_REMOCON_SUPPORT
			if (m_App.pNetworkRemocon!=nullptr)
				m_App.pNetworkRemocon->SetService(ServiceIndex);
#endif

			//AddLog(TEXT("�T�[�r�X��ύX���܂����B(SID %d)"),ServiceID);

			if (fStrict && m_f1SegMode
					&& !m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.Is1SegService(ServiceIndex)) {
				Set1SegMode(false,false);
			}
		}
	}

	if (m_f1SegMode && !ServiceSel.bPrefer1Seg) {
		Set1SegMode(false,false);
	}

	bool fChannelChanged=false;

	if (!m_f1SegMode && ServiceID!=0 && pCurChInfo!=nullptr) {
		const CChannelList *pChList=m_App.ChannelManager.GetCurrentRealChannelList();
		int Index=pChList->FindByIndex(pCurChInfo->GetSpace(),
									   pCurChInfo->GetChannelIndex(),
									   ServiceID);
		if (Index>=0) {
			m_App.ChannelManager.SetCurrentChannel(m_App.ChannelManager.GetCurrentSpace(),Index);
			m_App.UICore.OnChannelChanged(0);
			m_App.PluginManager.SendChannelChangeEvent();
			fChannelChanged=true;
		}
	}

	if (!fChannelChanged)
		m_App.UICore.OnServiceChanged();

	return true;
}


bool CAppCore::SetServiceByIndex(int Service,unsigned int Flags)
{
	if (Service < 0)
		return false;

	WORD ServiceID;

	if (!m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetViewableServiceID(Service,&ServiceID))
		return false;

	return SetServiceByID(ServiceID,Flags);
}


bool CAppCore::GetCurrentStreamIDInfo(StreamIDInfo *pInfo) const
{
	if (pInfo==nullptr)
		return false;

	pInfo->NetworkID=
		m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetNetworkID();
	pInfo->TransportStreamID=
		m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetTransportStreamID();
	WORD ServiceID;
	if (!m_App.CoreEngine.m_DtvEngine.GetServiceID(&ServiceID)) {
		int CurServiceID=m_App.ChannelManager.GetCurrentServiceID();
		if (CurServiceID>0)
			ServiceID=(WORD)CurServiceID;
		else
			ServiceID=0;
	}
	pInfo->ServiceID=ServiceID;

	return true;
}


bool CAppCore::GetCurrentStreamChannelInfo(CChannelInfo *pInfo) const
{
	if (pInfo==nullptr)
		return false;

	StreamIDInfo IDInfo;
	if (!GetCurrentStreamIDInfo(&IDInfo))
		return false;

	const CChannelInfo *pCurChInfo=m_App.ChannelManager.GetCurrentChannelInfo();
	if (pCurChInfo!=nullptr
			&& pCurChInfo->GetNetworkID()==IDInfo.NetworkID
			&& pCurChInfo->GetTransportStreamID()==IDInfo.TransportStreamID
			&& pCurChInfo->GetServiceID()==IDInfo.ServiceID) {
		*pInfo=*pCurChInfo;
	} else {
		CChannelInfo ChInfo;
		ChInfo.SetNetworkID(IDInfo.NetworkID);
		ChInfo.SetTransportStreamID(IDInfo.TransportStreamID);
		ChInfo.SetServiceID(IDInfo.ServiceID);
		*pInfo=ChInfo;
	}

	return true;
}


bool CAppCore::GetCurrentServiceName(LPTSTR pszName,int MaxLength,bool fUseChannelName)
{
	if (pszName==nullptr || MaxLength<1)
		return false;

	WORD ServiceID;
	if (!m_App.CoreEngine.m_DtvEngine.GetServiceID(&ServiceID))
		ServiceID=0;

	const CChannelInfo *pChannelInfo=nullptr;
	if (fUseChannelName) {
		pChannelInfo=m_App.ChannelManager.GetCurrentChannelInfo();
		if (pChannelInfo!=nullptr) {
			if (ServiceID==0 || pChannelInfo->GetServiceID()<=0
					|| pChannelInfo->GetServiceID()==ServiceID) {
				::lstrcpyn(pszName,pChannelInfo->GetName(),MaxLength);
				return true;
			}
		}
	}

	pszName[0]=_T('\0');

	if (ServiceID==0)
		return false;

	int Index=m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetServiceIndexByID(ServiceID);
	if (Index<0)
		return false;
#if 0
	if (pChannelInfo!=nullptr) {
		int Length=StdUtil::snprintf(pszName,MaxLength,TEXT("#%d "),Index+1);
		pszName+=Length;
		MaxLength-=Length;
	}
#endif
	if (m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetServiceName(Index,pszName,MaxLength)<1)
		return false;

	return true;
}


bool CAppCore::OpenTuner(LPCTSTR pszFileName)
{
	if (IsStringEmpty(pszFileName))
		return false;
	if (m_App.CoreEngine.IsTunerOpen()
			&& IsEqualFileName(m_App.CoreEngine.GetDriverFileName(),pszFileName))
		return true;

	TRACE(TEXT("CAppCore::OpenTuner(%s)\n"),pszFileName);

	HCURSOR hcurOld=::SetCursor(::LoadCursor(nullptr,IDC_WAIT));
	bool fOK;

	SaveCurrentChannel();
	SaveChannelSettings();

	m_App.CoreEngine.m_DtvEngine.SetTracer(&m_App.StatusView);

	if (m_App.CoreEngine.IsTunerOpen()) {
		m_App.CoreEngine.CloseTuner();
	}

	if (m_App.CoreEngine.IsCasCardOpen()) {
		LPCTSTR pszOldDriver=m_App.CoreEngine.GetDriverFileName();
		if (m_App.DriverOptions.IsNoDescramble(pszFileName)
				|| m_App.CasLibraryManager.GetCasDeviceNameByTunerName(pszFileName)!=
					m_App.CasLibraryManager.GetCasDeviceNameByTunerName(pszOldDriver)
				|| m_App.CasLibraryManager.GetCasReaderNameByTunerName(pszFileName)!=
					m_App.CasLibraryManager.GetCasReaderNameByTunerName(pszOldDriver)) {
			if (m_App.CoreEngine.CloseCasCard()) {
				AddLog(TEXT("�J�[�h���[�_�[����܂����B"));
			}
		}
	}

	m_App.CoreEngine.SetDriverFileName(pszFileName);
	fOK=OpenAndInitializeTuner();
	if (fOK) {
		InitializeChannel();
		m_App.PluginManager.SendDriverChangeEvent();
		::SetCursor(hcurOld);
	} else {
		m_App.PluginManager.SendDriverChangeEvent();
		::SetCursor(hcurOld);
		OnError(&m_App.CoreEngine,TEXT("BonDriver�̏��������ł��܂���B"));
	}

	m_App.CoreEngine.m_DtvEngine.SetTracer(nullptr);
	m_App.StatusView.SetSingleText(nullptr);
	m_App.UICore.OnTunerChanged();

	return fOK;
}


bool CAppCore::OpenTunerAndSetChannel(LPCTSTR pszDriverFileName,const CChannelInfo *pChannelInfo)
{
	if (IsStringEmpty(pszDriverFileName) || pChannelInfo==nullptr)
		return false;

	if (!m_App.UICore.ConfirmChannelChange())
		return false;

	if (!OpenTuner(pszDriverFileName))
		return false;

	const CChannelList *pList=m_App.ChannelManager.GetChannelList(pChannelInfo->GetSpace());
	if (pList==nullptr)
		return false;
	int Index=pList->FindByIndex(-1,
								 pChannelInfo->GetChannelIndex(),
								 pChannelInfo->GetServiceID());
	if (Index<0) {
		if (pChannelInfo->GetServiceID()!=0
				&& pChannelInfo->GetTransportStreamID()!=0) {
			Index=pList->FindByIDs(pChannelInfo->GetNetworkID(),
								   pChannelInfo->GetTransportStreamID(),
								   pChannelInfo->GetServiceID());
		}
		if (Index<0)
			return false;
	}

	return SetChannel(pChannelInfo->GetSpace(),Index);
}


bool CAppCore::OpenTuner()
{
	TRACE(TEXT("CAppCore::OpenTuner()\n"));

	if (!m_App.CoreEngine.IsDriverSpecified())
		return false;

	m_App.CoreEngine.m_DtvEngine.SetTracer(&m_App.StatusView);
	bool fOK=true;

	if (!m_App.CoreEngine.IsTunerOpen()) {
		if (OpenAndInitializeTuner(OPEN_CAS_CARD_NO_UI)) {
			m_App.UICore.OnTunerOpened();
		} else {
			OnError(&m_App.CoreEngine,TEXT("BonDriver�̏��������ł��܂���B"));
			fOK=false;
		}
	}

	m_App.CoreEngine.m_DtvEngine.SetTracer(nullptr);
	m_App.StatusView.SetSingleText(nullptr);

	return fOK;
}


bool CAppCore::OpenAndInitializeTuner(unsigned int OpenCasCardFlags)
{
	CDriverOptions::BonDriverOptions Options(m_App.CoreEngine.GetDriverFileName());
	m_App.DriverOptions.GetBonDriverOptions(m_App.CoreEngine.GetDriverFileName(),&Options);

	m_App.CoreEngine.m_DtvEngine.SetStartStreamingOnDriverOpen(!Options.fIgnoreInitialStream);

	if (!m_App.CoreEngine.OpenTuner())
		return false;

	AddLog(TEXT("%s ��ǂݍ��݂܂����B"),m_App.CoreEngine.GetDriverFileName());

	ApplyBonDriverOptions();

	if (!m_App.CasLibraryManager.HasCasLibraryNetworkMap()
			&& m_App.CoreEngine.IsCasLibraryLoaded()
			&& !m_App.CoreEngine.IsCasCardOpen()
			&& !m_App.DriverOptions.IsNoDescramble(m_App.CoreEngine.GetDriverFileName())) {
		OpenCasCard(OpenCasCardFlags);
	}

	return true;
}


bool CAppCore::CloseTuner()
{
	TRACE(TEXT("CAppCore::CloseTuner()\n"));

	if (m_App.CoreEngine.IsCasCardOpen()) {
		if (m_App.CoreEngine.CloseCasCard()) {
			AddLog(TEXT("�J�[�h���[�_�[����܂����B"));
		}
	}

	if (m_App.CoreEngine.IsTunerOpen()) {
		m_App.CoreEngine.CloseTuner();
		SaveCurrentChannel();
		m_App.ChannelManager.SetCurrentChannel(m_App.ChannelManager.GetCurrentSpace(),-1);
		m_App.UICore.OnTunerClosed();
	}

	return true;
}


bool CAppCore::Set1SegMode(bool f1Seg,bool fServiceChange)
{
	if (m_f1SegMode != f1Seg) {
		m_f1SegMode = f1Seg;

		AddLog(TEXT("�����Z�O���[�h��%s�ɂ��܂��B"),f1Seg?TEXT("�I��"):TEXT("�I�t"));

		if (fServiceChange) {
			if (m_f1SegMode) {
				CTsAnalyzer &TsAnalyzer=m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer;

				if (TsAnalyzer.Has1SegService()) {
					WORD ServiceID=CDtvEngine::SID_DEFAULT;
					CChannelInfo ChInfo;

					if (GetCurrentStreamChannelInfo(&ChInfo)) {
						int Index=GetCorresponding1SegService(
							ChInfo.GetSpace(),
							ChInfo.GetNetworkID(),
							ChInfo.GetTransportStreamID(),
							ChInfo.GetServiceID());
						WORD SID;
						if (TsAnalyzer.Get1SegServiceIDByIndex(Index,&SID))
							ServiceID=SID;
					}

					SetServiceByID(ServiceID,SET_SERVICE_NO_CHANGE_CUR_SERVICE_ID);
				} else if (TsAnalyzer.GetViewableServiceNum()>0) {
					m_f1SegMode=false;
					AddLog(TEXT("�����Z�O�T�[�r�X������܂���B"));
					if (!m_fSilent) {
						m_App.UICore.GetSkin()->ShowMessage(
							TEXT("�����Z�O�T�[�r�X������܂���B"),TEXT("�����Z�O���[�h"),
							MB_OK | MB_ICONINFORMATION);
					}
					return false;
				}
			} else {
				SetServiceByID(
					m_App.ChannelManager.GetCurrentServiceID()>0?
						m_App.ChannelManager.GetCurrentServiceID():CDtvEngine::SID_DEFAULT,
					SET_SERVICE_NO_CHANGE_CUR_SERVICE_ID);
			}
		}

		m_App.UICore.On1SegModeChanged(m_f1SegMode);
	}

	return true;
}


int CAppCore::GetCorresponding1SegService(
	int Space,WORD NetworkID,WORD TSID,WORD ServiceID) const
{
	if (ServiceID==0)
		return 0;

	int ServiceIndex=0;

	for (WORD SID=ServiceID-1;SID>0;SID--) {
		if (m_App.ChannelManager.FindChannelByIDs(Space,NetworkID,TSID,SID,false)<0)
			break;
		ServiceIndex=ServiceID-SID;
	}

	return ServiceIndex;
}


bool CAppCore::OpenCasCard(unsigned int Flags)
{
	TRACE(TEXT("CAppCore::OpenCasCard()\n"));

	if (!m_App.CoreEngine.IsCasCardOpen()) {
		int Device;
		const TVTest::String &DeviceName=
			m_App.CasLibraryManager.GetCasDeviceNameByTunerName(m_App.CoreEngine.GetDriverFileName());
		if (!DeviceName.empty()) {
			Device=m_App.CoreEngine.m_DtvEngine.m_CasProcessor.GetCasDeviceByName(DeviceName.c_str());
			if (Device<0)
				AddLog(TEXT("�w�肳�ꂽCAS�f�o�C�X \"%s\" ������܂���B"),DeviceName.c_str());
		} else {
			Device=m_App.GeneralOptions.GetCasDevice(true);
		}

		if (Device>=0 && !OpenCasCardDefaultReader(Device)) {
			AddLog(TEXT("�J�[�h���[�_�[���I�[�v���ł��܂���B"));

			m_fCasCardOpenError=true;

			if ((Flags & OPEN_CAS_CARD_RETRY)!=0) {
				TCHAR szText[1024];
				CStaticStringFormatter Formatter(szText,lengthof(szText));
				CCardReaderErrorDialog Dialog;

				Formatter.AppendFormat(TEXT("%s\r\n"),m_App.CoreEngine.GetLastErrorText());
				if (!IsStringEmpty(m_App.CoreEngine.GetLastErrorSystemMessage()))
					Formatter.AppendFormat(TEXT("(%s)\r\n"),m_App.CoreEngine.GetLastErrorSystemMessage());
				Formatter.Append(
					TEXT("����������Ɏ����ł���̂ɂ��̃_�C�A���O���\�������ꍇ�A")
					TEXT("�ݒ�ŃJ�[�h���[�_�[�Ɂu�Ȃ��v��I�����Ă��������B"));
				Dialog.SetMessage(Formatter.GetString());
				while (Dialog.Show(m_App.UICore.GetDialogOwner())) {
					if (m_App.CoreEngine.OpenCasCard(Dialog.GetCasDevice(),Dialog.GetReaderName())
							|| Dialog.GetCasDevice()<0)
						break;
				}
			} else {
				if ((Flags & OPEN_CAS_CARD_NOTIFY_ERROR)!=0) {
					m_App.UICore.GetSkin()->ShowNotificationBar(
						TEXT("�J�[�h���[�_�[���I�[�v���ł��܂���B"),
						CNotificationBar::MESSAGE_ERROR,6000);
				}

				if ((Flags & OPEN_CAS_CARD_NO_UI)==0 && !m_fSilent) {
					m_App.UICore.GetSkin()->ShowErrorMessage(&m_App.CoreEngine);
				}
			}
		}

		m_fCasCardOpenError=!m_App.CoreEngine.IsCasCardOpen();

		OutCasCardInfo();
	}

	return true;
}


bool CAppCore::ChangeCasCard(int Device,LPCTSTR pszName)
{
	bool fResult;

	if (pszName==nullptr) {
		fResult=OpenCasCardDefaultReader(Device);
	} else {
		fResult=m_App.CoreEngine.OpenCasCard(Device,pszName);
	}
	if (!fResult) {
		OnError(&m_App.CoreEngine);
		return false;
	}

	OutCasCardInfo();

	return true;
}


bool CAppCore::LoadCasLibrary(LPCTSTR pszFileName)
{
	if (IsStringEmpty(pszFileName)) {
		if (!m_App.CasLibraryManager.HasDefaultCasLibrary())
			return false;
		pszFileName=m_App.CasLibraryManager.GetDefaultCasLibrary();
	}

	TRACE(TEXT("CAppCore::LoadCasLibrary() : \"%s\"\n"),pszFileName);

	m_fCasCardOpenError=false;

	m_App.CoreEngine.SetCasLibraryName(pszFileName);

	if (!m_App.CoreEngine.LoadCasLibrary()) {
		OnError(&m_App.CoreEngine,TEXT("CAS���C�u������ǂݍ��߂܂���B"));
		return false;
	}

	CCasProcessor::CasModuleInfo ModInfo;

	m_App.CoreEngine.m_DtvEngine.m_CasProcessor.GetCasModuleInfo(&ModInfo);
	AddLog(TEXT("CAS���C�u���� \"%s\" (%s %s) ��ǂݍ��݂܂����B"),
		   m_App.CoreEngine.GetCasLibraryName(),
		   ModInfo.Name,ModInfo.Version);

	return true;
}


bool CAppCore::LoadCasLibrary()
{
	WORD NetworkID,TSID;

	NetworkID=m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetNetworkID();
	TSID=m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetTransportStreamID();
	if (NetworkID==0 || TSID==0)
		return false;

	return LoadCasLibrary(NetworkID,TSID);
}


bool CAppCore::LoadCasLibrary(WORD NetworkID,WORD TSID)
{
	const TVTest::String &FileName=m_App.CasLibraryManager.GetCasLibraryFileName(NetworkID,TSID);
	if (FileName.empty())
		return false;
	if (IsEqualFileName(FileName.c_str(),m_App.CoreEngine.GetCasLibraryName()))
		return !m_fCasCardOpenError;

	return LoadCasLibrary(FileName.c_str());
}


bool CAppCore::OpenCasCardDefaultReader(int Device)
{
	const TVTest::String &ReaderName=
		m_App.CasLibraryManager.GetCasReaderNameByTunerName(m_App.CoreEngine.GetDriverFileName());

	return m_App.CoreEngine.OpenCasCard(Device,ReaderName.empty()?nullptr:ReaderName.c_str());
}


void CAppCore::OutCasCardInfo()
{
	if (m_App.CoreEngine.IsCasCardOpen()) {
		TCHAR szName[MAX_PATH];
		if (m_App.CoreEngine.m_DtvEngine.m_CasProcessor.GetCasCardName(szName,lengthof(szName))>0) {
			AddLog(TEXT("�J�[�h���[�_�[ \"%s\" ���I�[�v�����܂����B"),szName);
			CCasProcessor::CasCardInfo CardInfo;
			if (m_App.CoreEngine.m_DtvEngine.m_CasProcessor.GetCasCardInfo(&CardInfo)) {
				AddLog(TEXT("(�J�[�hID %s / �J�[�h���� %c%03d)"),
					CardInfo.CardIDText,CardInfo.CardManufacturerID,CardInfo.CardVersion);
			}
		}
	}
}


void CAppCore::ApplyBonDriverOptions()
{
	if (!m_App.CoreEngine.IsTunerOpen())
		return;

	CDriverOptions::BonDriverOptions Options(m_App.CoreEngine.GetDriverFileName());
	m_App.DriverOptions.GetBonDriverOptions(m_App.CoreEngine.GetDriverFileName(),&Options);

	//m_App.CoreEngine.m_DtvEngine.SetStartStreamingOnDriverOpen(!Options.fIgnoreInitialStream);

	CBonSrcDecoder &BonSrcDecoder=m_App.CoreEngine.m_DtvEngine.m_BonSrcDecoder;

	BonSrcDecoder.SetPurgeStreamOnChannelChange(Options.fPurgeStreamOnChannelChange);
	BonSrcDecoder.SetFirstChannelSetDelay(Options.FirstChannelSetDelay);
	BonSrcDecoder.SetMinChannelChangeInterval(Options.MinChannelChangeInterval);
}


bool CAppCore::SaveCurrentChannel()
{
	if (!IsStringEmpty(m_App.CoreEngine.GetDriverFileName())) {
		const CChannelInfo *pInfo=m_App.ChannelManager.GetCurrentRealChannelInfo();

		if (pInfo!=nullptr) {
			CDriverOptions::ChannelInfo ChInfo;

			ChInfo.Space=pInfo->GetSpace();
			ChInfo.Channel=pInfo->GetChannelIndex();
			ChInfo.ServiceID=pInfo->GetServiceID();
			ChInfo.TransportStreamID=pInfo->GetTransportStreamID();
			ChInfo.fAllChannels=m_App.ChannelManager.GetCurrentSpace()==CChannelManager::SPACE_ALL;
			m_App.DriverOptions.SetLastChannel(m_App.CoreEngine.GetDriverFileName(),&ChInfo);
		}
	}
	return true;
}


bool CAppCore::ShowHelpContent(int ID)
{
	return m_App.HtmlHelpClass.ShowContent(ID);
}


bool CAppCore::GenerateRecordFileName(LPTSTR pszFileName,int MaxFileName) const
{
	CRecordManager::EventInfo EventInfo;
	const CChannelInfo *pChannelInfo=m_App.ChannelManager.GetCurrentChannelInfo();
	WORD ServiceID;
	TCHAR szServiceName[32],szEventName[256];

	EventInfo.pszChannelName=nullptr;
	EventInfo.ChannelNo=0;
	EventInfo.pszServiceName=nullptr;
	EventInfo.pszEventName=nullptr;
	if (pChannelInfo!=nullptr) {
		EventInfo.pszChannelName=pChannelInfo->GetName();
		if (pChannelInfo->GetChannelNo()!=0)
			EventInfo.ChannelNo=pChannelInfo->GetChannelNo();
		else if (pChannelInfo->GetServiceID()>0)
			EventInfo.ChannelNo=pChannelInfo->GetServiceID();
	}
	if (m_App.CoreEngine.m_DtvEngine.GetServiceID(&ServiceID)) {
		int Index=m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetServiceIndexByID(ServiceID);
		if (m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetServiceName(Index,szServiceName,lengthof(szServiceName)))
			EventInfo.pszServiceName=szServiceName;
		if (!m_App.CoreEngine.m_DtvEngine.GetServiceID(&EventInfo.ServiceID))
			EventInfo.ServiceID=0;
		bool fNext=false;
		SYSTEMTIME stCur,stStart;
		if (!m_App.CoreEngine.m_DtvEngine.m_TsAnalyzer.GetTotTime(&stCur))
			GetCurrentJST(&stCur);
		if (m_App.CoreEngine.m_DtvEngine.GetEventTime(&stStart,nullptr,true)) {
			LONGLONG Diff=DiffSystemTime(&stCur,&stStart);
			if (Diff>=0 && Diff<60*1000)
				fNext=true;
		}
		if (m_App.CoreEngine.m_DtvEngine.GetEventName(szEventName,lengthof(szEventName),fNext))
			EventInfo.pszEventName=szEventName;
		EventInfo.EventID=m_App.CoreEngine.m_DtvEngine.GetEventID(fNext);
		EventInfo.stTotTime=stCur;
	}
	return m_App.RecordManager.GenerateFileName(pszFileName,MaxFileName,&EventInfo);
}


bool CAppCore::StartRecord(LPCTSTR pszFileName,
						   const CRecordManager::TimeSpecInfo *pStartTime,
						   const CRecordManager::TimeSpecInfo *pStopTime,
						   CRecordManager::RecordClient Client,
						   bool fTimeShift)
{
	if (m_App.RecordManager.IsRecording())
		return false;
	m_App.RecordManager.SetFileName(pszFileName);
	m_App.RecordManager.SetStartTimeSpec(pStartTime);
	m_App.RecordManager.SetStopTimeSpec(pStopTime);
	m_App.RecordManager.SetStopOnEventEnd(false);
	m_App.RecordManager.SetClient(Client);
	m_App.RecordOptions.ApplyOptions(&m_App.RecordManager);
	if (m_App.CmdLineOptions.m_fRecordCurServiceOnly)
		m_App.RecordManager.SetCurServiceOnly(true);
	if (m_App.RecordManager.IsReserved()) {
		m_App.StatusView.UpdateItem(STATUS_ITEM_RECORD);
		return true;
	}

	OpenTuner();

	TCHAR szFileName[MAX_PATH*2];
	if (IsStringEmpty(pszFileName)) {
		LPCTSTR pszErrorMessage;

		if (!m_App.RecordOptions.GenerateFilePath(szFileName,lengthof(szFileName),
												  &pszErrorMessage)) {
			m_App.UICore.GetSkin()->ShowErrorMessage(pszErrorMessage);
			return false;
		}
		m_App.RecordManager.SetFileName(szFileName);
	}
	if (!GenerateRecordFileName(szFileName,lengthof(szFileName)))
		return false;
	m_App.PluginManager.SendStartRecordEvent(&m_App.RecordManager,szFileName,lengthof(szFileName));
	m_App.CoreEngine.ResetErrorCount();
	if (!m_App.RecordManager.StartRecord(&m_App.CoreEngine.m_DtvEngine,szFileName,fTimeShift)) {
		OnError(&m_App.RecordManager,TEXT("�^����J�n�ł��܂���B"));
		return false;
	}
	m_App.ResidentManager.SetStatus(CResidentManager::STATUS_RECORDING,
									CResidentManager::STATUS_RECORDING);
	AddLog(TEXT("�^��J�n %s"),szFileName);
	m_App.UICore.OnRecordingStarted();
	return true;
}


bool CAppCore::ModifyRecord(LPCTSTR pszFileName,
							const CRecordManager::TimeSpecInfo *pStartTime,
							const CRecordManager::TimeSpecInfo *pStopTime,
							CRecordManager::RecordClient Client)
{
	m_App.RecordManager.SetFileName(pszFileName);
	m_App.RecordManager.SetStartTimeSpec(pStartTime);
	m_App.RecordManager.SetStopTimeSpec(pStopTime);
	m_App.RecordManager.SetClient(Client);
	m_App.StatusView.UpdateItem(STATUS_ITEM_RECORD);
	return true;
}


bool CAppCore::StartReservedRecord()
{
	TCHAR szFileName[MAX_PATH];

	/*
	if (!m_App.RecordManager.IsReserved())
		return false;
	*/
	if (!IsStringEmpty(m_App.RecordManager.GetFileName())) {
		if (!GenerateRecordFileName(szFileName,MAX_PATH))
			return false;
		/*
		if (!m_App.RecordManager.DoFileExistsOperation(m_App.UICore.GetDialogOwner(),szFileName))
			return false;
		*/
	} else {
		LPCTSTR pszErrorMessage;

		if (!m_App.RecordOptions.GenerateFilePath(szFileName,lengthof(szFileName),
												  &pszErrorMessage)) {
			m_App.UICore.GetSkin()->ShowErrorMessage(pszErrorMessage);
			return false;
		}
		m_App.RecordManager.SetFileName(szFileName);
		if (!GenerateRecordFileName(szFileName,MAX_PATH))
			return false;
	}
	OpenTuner();
	m_App.PluginManager.SendStartRecordEvent(&m_App.RecordManager,szFileName,lengthof(szFileName));
	m_App.CoreEngine.ResetErrorCount();
	if (!m_App.RecordManager.StartRecord(&m_App.CoreEngine.m_DtvEngine,szFileName)) {
		m_App.RecordManager.CancelReserve();
		OnError(&m_App.RecordManager,TEXT("�^����J�n�ł��܂���B"));
		return false;
	}
	m_App.ResidentManager.SetStatus(CResidentManager::STATUS_RECORDING,
									CResidentManager::STATUS_RECORDING);
	AddLog(TEXT("�^��J�n %s"),szFileName);
	m_App.UICore.OnRecordingStarted();
	return true;
}


bool CAppCore::CancelReservedRecord()
{
	if (!m_App.RecordManager.CancelReserve())
		return false;
	m_App.StatusView.UpdateItem(STATUS_ITEM_RECORD);
	return true;
}


bool CAppCore::StopRecord()
{
	if (!m_App.RecordManager.IsRecording())
		return false;

	TCHAR szFileName[MAX_PATH],szSize[32];
	StdUtil::strncpy(szFileName,lengthof(szFileName),
					 m_App.RecordManager.GetRecordTask()->GetFileName());

	m_App.RecordManager.StopRecord();
	m_App.RecordOptions.Apply(CRecordOptions::UPDATE_RECORDSTREAM);
	m_App.CoreEngine.m_DtvEngine.SetDescrambleCurServiceOnly(
		m_App.GeneralOptions.GetDescrambleCurServiceOnly());

	UInt64ToString(m_App.CoreEngine.m_DtvEngine.m_FileWriter.GetWriteSize(),
				   szSize,lengthof(szSize));
	AddLog(TEXT("�^���~ %s (�����o���T�C�Y %s Bytes)"),szFileName,szSize);

	m_App.ResidentManager.SetStatus(0,CResidentManager::STATUS_RECORDING);
	m_App.UICore.OnRecordingStopped();
	if (m_fExitOnRecordingStop)
		m_App.Exit();

	return true;
}


bool CAppCore::RelayRecord(LPCTSTR pszFileName)
{
	if (IsStringEmpty(pszFileName) || !m_App.RecordManager.IsRecording())
		return false;
	if (!m_App.RecordManager.RelayFile(pszFileName)) {
		OnError(&m_App.RecordManager,TEXT("�^��t�@�C����؂�ւ��ł��܂���B"));
		return false;
	}
	AddLog(TEXT("�^��t�@�C����؂�ւ��܂��� %s"),pszFileName);
	m_App.PluginManager.SendRelayRecordEvent(pszFileName);
	return true;
}


bool CAppCore::CommandLineRecord(const CCommandLineOptions *pCmdLine)
{
	if (pCmdLine==nullptr)
		return false;
	return CommandLineRecord(pCmdLine->m_RecordFileName.Get(),
							 &pCmdLine->m_RecordStartTime,
							 pCmdLine->m_RecordDelay,
							 pCmdLine->m_RecordDuration);

}


bool CAppCore::CommandLineRecord(LPCTSTR pszFileName,const FILETIME *pStartTime,int Delay,int Duration)
{
	CRecordManager::TimeSpecInfo StartTime,StopTime;

	if (pStartTime!=nullptr && (pStartTime->dwLowDateTime!=0 || pStartTime->dwHighDateTime!=0)) {
		StartTime.Type=CRecordManager::TIME_DATETIME;
		StartTime.Time.DateTime=*pStartTime;
		if (Delay!=0)
			StartTime.Time.DateTime+=(LONGLONG)Delay*FILETIME_SECOND;
		SYSTEMTIME st;
		::FileTimeToSystemTime(pStartTime,&st);
		AddLog(TEXT("�R�}���h���C������^��w�肳��܂����B(%d/%d/%d %d:%02d:%02d �J�n)"),
			   st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
	} else if (Delay>0) {
		StartTime.Type=CRecordManager::TIME_DURATION;
		StartTime.Time.Duration=Delay*1000;
		AddLog(TEXT("�R�}���h���C������^��w�肳��܂����B(%d �b��J�n)"),Delay);
	} else {
		StartTime.Type=CRecordManager::TIME_NOTSPECIFIED;
		AddLog(TEXT("�R�}���h���C������^��w�肳��܂����B"));
	}
	if (Duration>0) {
		StopTime.Type=CRecordManager::TIME_DURATION;
		StopTime.Time.Duration=Duration*1000;
	} else {
		StopTime.Type=CRecordManager::TIME_NOTSPECIFIED;
	}

	return StartRecord(
		IsStringEmpty(pszFileName)?nullptr:pszFileName,
		&StartTime,&StopTime,
		CRecordManager::CLIENT_COMMANDLINE);
}


LPCTSTR CAppCore::GetDefaultRecordFolder() const
{
	return m_App.RecordOptions.GetSaveFolder();
}


void CAppCore::BeginChannelScan(int Space)
{
	m_App.ChannelManager.SetCurrentChannel(Space,-1);
	m_App.UICore.OnChannelChanged(CUICore::CHANNEL_CHANGED_STATUS_SPACE_CHANGED);
}


bool CAppCore::IsChannelScanning() const
{
	return m_App.ChannelScan.IsScanning();
}


bool CAppCore::IsDriverNoSignalLevel(LPCTSTR pszFileName) const
{
	return m_App.DriverOptions.IsNoSignalLevel(pszFileName);
}


void CAppCore::SetProgress(int Pos,int Max)
{
	m_App.TaskbarManager.SetProgress(Pos,Max);
}


void CAppCore::EndProgress()
{
	m_App.TaskbarManager.EndProgress();
}


COLORREF CAppCore::GetColor(LPCTSTR pszText) const
{
	return m_App.ColorSchemeOptions.GetColor(pszText);
}