#include "stdafx.h"
#include "DlgProgress.h"
#include "Helpers.h"
#include "Archive.h"
#include "Settings.h"
#include "MainFrm.h"

#pragma warning( push )
#pragma warning(disable:4477) //string '%d' requires an argument of type 'int *', but variadic argument 2 has type 'char *'

DlgProgress::DlgProgress(WORD cmd, LPSTR lpstrFile)
{
    m_cmd = cmd;
    m_pTaskThread = new TaskThread(cmd, lpstrFile);
}


DlgProgress::~DlgProgress()
{
    delete m_pTaskThread;
}

LRESULT DlgProgress::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    if (!m_pTaskThread->IsOK())
    {
        End(IDCANCEL);
        return FALSE;
    }

    if (m_cmd == ID_FILE_IMPORT)
        ::SendMessage(hwndMain, WM_INPORT_TASK, 0, 0);
    m_pTaskThread->StartWork(NULL);
    SetTimer(1, 2000);

    m_ctrlInfo.Attach(GetDlgItem(IDC_STATIC_INFO));
    m_ctrlProgress.Attach(GetDlgItem(IDC_PROGRESS));
    m_ctrlProgress.SetRange(0, 100);

    CenterWindow(GetParent());
    return TRUE;
}

LRESULT DlgProgress::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    if (wParam == 1)
    {
        if (m_pTaskThread->IsWorking())
        {
            double total, cur;
            m_pTaskThread->GetProgress(total, cur);
            CString strMsg;
            strMsg.Format(_T("%llu of %llu"), (long long)cur, (long long)total);
            m_ctrlInfo.SetWindowText(strMsg);
            if (total)
            {
                double progress = 100.0 * (cur / total);
                m_ctrlProgress.SetPos((int)progress);
                Helpers::UpdateStatusBar();
            }

        }
        else
        {
            End(IDOK);
        }

    }
    return 0;
}

LRESULT DlgProgress::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    End(IDCANCEL);
    return 0;
}

void DlgProgress::End(int wID)
{
    KillTimer(1);
    m_pTaskThread->StopWork();
    if (m_cmd == ID_FILE_IMPORT)
    {
        ::PostMessage(hwndMain, WM_INPORT_TASK, 1, 0);
    }
    EndDialog(wID);

}

///////////////////////////////////////////////////
TaskThread::TaskThread(WORD cmd, LPSTR lpstrFile)
{
    m_cmd = cmd;
    m_isOK = false;
    m_fp = NULL;
    m_progress = 0;
    m_count = 1;

    if (lpstrFile == 0)
    {
        INT_PTR nRet;
        if (m_cmd == ID_FILE_IMPORT)
        {
            CFileDialog dlg(TRUE);
            nRet = dlg.DoModal();
            m_strFile = dlg.m_ofn.lpstrFile;
        }
        else
        {
            //LPCTSTR lpcstrTextFilter =
            //  _T("Text Files (*.txt)\0*.txt\0")
            //  _T("All Files (*.*)\0*.*\0")
            //  _T("");
            CFileDialog dlg(FALSE);// , NULL, _T(""), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, lpcstrTextFilter);
            nRet = dlg.DoModal();
            m_strFile = dlg.m_ofn.lpstrFile;
        }
    }
    else
    {
        m_strFile = lpstrFile;
    }

    m_isAuto = false;
    if (!m_strFile.IsEmpty())
    {
        if (m_strFile == "auto")
        {
            m_isAuto = true;
        }
        else
        {
            if (m_cmd == ID_FILE_IMPORT)
            {
                if (0 != fopen_s(&m_fp, m_strFile, "r")) {
                    Helpers::SysErrMessageBox(TEXT("Cannot open file %s"), m_strFile);
                    m_fp = NULL;
                }
            }
            else
            {
                if (0 != fopen_s(&m_fp, m_strFile, "w")) {
                    Helpers::SysErrMessageBox(TEXT("Cannot create file %s"), m_strFile);
                    m_fp = NULL;
                }
            }
        }
    }
    m_isOK = m_isAuto || (m_fp != NULL);

}

TaskThread::~TaskThread()
{
    if (m_fp)
        fclose(m_fp);
}

void TaskThread::Terminate()
{
}

void TaskThread::GetProgress(double& total, double& cur)
{
    total = m_count;
    cur = m_progress;
}

void TaskThread::Work(LPVOID pWorkParam)
{
    if (m_isOK)
    {
        if (m_cmd == ID_FILE_IMPORT)
            FileImport();
        else
            FileSave(m_cmd);
    }
}

void TaskThread::FileSave(WORD cmd)
{
    bool isExport = (cmd == ID_FILE_EXPORT);

    m_count = isExport ? gArchive.getCount() : gArchive.getListedNodes()->Count();

    char buf[MAX_RECORD_LEN];
    ROW_LOG_REC* rec = (ROW_LOG_REC*)buf;

    for (DWORD i = 0; (i < m_count) && IsWorking(); i++, m_progress++)
    {
        LOG_NODE* pNode = isExport ? gArchive.getNode(i) : gArchive.getListedNodes()->getNode(i);
        if (!(pNode->isFlow() || pNode->isTrace()))
            continue;

		if (isExport)
        {
            int cbColor = 0;
            char szColor[32];
            if (pNode->isFlow())
            {
                FLOW_NODE* p = (FLOW_NODE*)pNode;
                rec->log_type = p->log_type;
				rec->log_flags = p->log_flags;
				rec->nn = p->nn;
                rec->sec = p->sec;
                rec->msec = p->msec;
                rec->tid = pNode->threadNode->tid;
                rec->pid = pNode->threadNode->pAppNode->pid;
                rec->this_fn = p->this_fn;
                rec->call_site = p->call_site;
				rec->fn_line = p->fn_line;
				rec->call_line = p->call_line;

                rec->cb_app_path = pNode->threadNode->pAppNode->cb_app_path;
                memcpy(rec->appPath(), pNode->threadNode->pAppNode->appPath(), rec->cb_app_path);

                rec->cb_fn_name = p->cb_fn_name;
                memcpy(rec->fnName(), p->fnName(), p->cb_fn_name);

                rec->cb_trace = 0;
            }
            else
            {
                TRACE_NODE* p = (TRACE_NODE*)pNode;
                rec->log_type = p->log_type;
				rec->log_flags = p->log_flags;
				rec->nn = p->nn;
                rec->sec = p->sec;
                rec->msec = p->msec;
                rec->tid = pNode->threadNode->tid;
                rec->this_fn = 0;
                rec->call_site = 0;
				rec->fn_line = p->fn_line;
				rec->call_line = p->call_line;

                rec->cb_app_path = pNode->threadNode->pAppNode->cb_app_path;
                memcpy(rec->appPath(), pNode->threadNode->pAppNode->appPath(), rec->cb_app_path);

                rec->cb_fn_name = p->cb_fn_name;
                memcpy(rec->fnName(), p->fnName(), p->cb_fn_name);

                char* log = pNode->getListText(&rec->cb_trace, LOG_COL);
                memcpy(rec->trace(), log, rec->cb_trace + 1);
                if (p->color)
                {
                    cbColor = sprintf_s(szColor, _countof(szColor), "\033[0;%dm", p->color);
                    rec->cb_trace += cbColor;
                }
            }
            rec->len = sizeof(ROW_LOG_REC) + rec->cbData();

            fprintf(m_fp, "%d %d %d %d %d %d %d %d %u %u %d %d %d %d-",
                rec->len, rec->log_type, rec->log_flags, rec->nn, rec->cb_app_path, 
				rec->cb_fn_name, rec->cb_trace, rec->tid, 
				rec->sec, rec->msec, 
				rec->this_fn, rec->call_site, rec->fn_line, rec->call_line);

            fwrite(rec->data, rec->cbData() - cbColor, 1, m_fp);
            if (cbColor)
                fwrite(szColor, cbColor, 1, m_fp);
            fwrite("\n", 1, 1, m_fp);
        }
        else
        {
            fwrite("[", 1, 1, m_fp);
            if (pNode->isFlow())
            {
                FLOW_NODE* p = (FLOW_NODE*)pNode;
                fwrite(pNode->threadNode->pAppNode->appPath(), 1, pNode->threadNode->pAppNode->cb_app_path, m_fp);
                fwrite(" ", 1, 1, m_fp);
                fwrite(p->fnName(), 1, p->cb_fn_name, m_fp);
                fprintf(m_fp, " %d", p->call_line);
            }
            else
            {
                TRACE_NODE* p = (TRACE_NODE*)pNode;
                fwrite(pNode->threadNode->pAppNode->appPath(), 1, pNode->threadNode->pAppNode->cb_app_path, m_fp);
                fwrite(" ", 1, 1, m_fp);
                fwrite(p->fnName(), 1, p->cb_fn_name, m_fp);
                fprintf(m_fp, " %d", p->call_line);
            }
            fwrite("] ", 1, 2, m_fp);

            if (pNode->isFlow())
                fwrite("\n", 1, 1, m_fp);
            else
                fprintf(m_fp, "%s\n", pNode->getListText(&rec->cb_trace, LOG_COL));
        }
    }
}

void TaskThread::FileImport()
{
    if (m_isAuto)
    {
#ifdef _BUILD_X64 
        m_count = 3;// 16LL * 1024 * 1024;// 128000000;
#else
        m_count = 3;// 16LL * 1024 * 1024;// 12800000;
#endif
        int cRecursion = 3;//10
        int ii = 0;
        bool ss = true;
        char sz1[] = { "84 0 2 3 1 23 0 1 0 0 -944928792 -2144780369 18 6-JHello.HelloWorld.<init>\n" };
        char sz2[] = { "87 2 2 3 1 23 3 1 0 0 -944928792 -2144780369 18 6-JHello.HelloWorld.123456123" };
        char sz3[] = { "84 1 2 3 1 23 0 1 0 0 -944928792 -2144780369 18 6-JHello.HelloWorld.<init>\n" };
        char buf1[MAX_RECORD_LEN / 2] = { 0 }, buf2[MAX_RECORD_LEN / 2] = { 0 }, buf3[MAX_RECORD_LEN / 2] = { 0 };
        ROW_LOG_REC* rec1 = (ROW_LOG_REC*)buf1;
        ROW_LOG_REC* rec2 = (ROW_LOG_REC*)buf2;
        ROW_LOG_REC* rec3 = (ROW_LOG_REC*)buf3;
		char *format = TEXT("%d %d %d %d %d %d %d %d %u %u %p %p %d %d-%s");
        ss = ((ii = sscanf_s(sz1, format, 
            &rec1->len, &rec1->log_type, &rec1->log_flags, &rec1->nn, &rec1->cb_app_path, 
			&rec1->cb_fn_name, &rec1->cb_trace, &rec1->tid, 
			&rec1->sec, &rec1->msec, 
			&rec1->this_fn, &rec1->call_site, &rec1->fn_line, &rec1->call_line, &rec1->data, 100)));
        ss = ((ii = sscanf_s(sz2, format,
            &rec2->len, &rec2->log_type, &rec1->log_flags, &rec2->nn, &rec2->cb_app_path, 
			&rec2->cb_fn_name, &rec2->cb_trace, &rec2->tid, 
			&rec2->sec, &rec2->msec, 
			&rec2->this_fn, &rec2->call_site, &rec1->fn_line, &rec2->call_line, &rec2->data, 100)));
        ss = ((ii = sscanf_s(sz3, format,
            &rec3->len, &rec3->log_type, &rec1->log_flags, &rec3->nn, &rec3->cb_app_path, 
			&rec3->cb_fn_name, &rec3->cb_trace, &rec3->tid, 
			&rec3->sec, &rec3->msec, 
			&rec3->this_fn, &rec3->call_site, &rec1->fn_line, &rec3->call_line, &rec3->data, 100)));
        rec1->len = rec1->size();
        rec2->len = rec2->size(); rec2->trace()[rec2->cb_trace - 1] = '\n';
        rec3->len = rec3->size();
        ss = ss && rec1->isValid();
        ss = ss && rec2->isValid();
        for (DWORD j = 0; ss && j < m_count && IsWorking(); j++)
        {
            m_progress = j;
            for (int i = 0; i < cRecursion; i++)
            {
                ss = ss && gArchive.append(rec1);
                ss = ss && gArchive.append(rec2);
            }
            for (int i = 0; i < cRecursion; i++)
                ss = ss && gArchive.append(rec3);
        }
    }
    else
    {
        char buf[MAX_RECORD_LEN];

        DWORD count = 0;
        fseek(m_fp, 0, SEEK_END);
        m_count = ftell(m_fp);
        fseek(m_fp, 0, SEEK_SET);
        bool ss = true;
        bool appended = true;
        while (ss && IsWorking())
        {
            count++;
            if (count == 2656)
                count = count;
            ROW_LOG_REC* rec = (ROW_LOG_REC*)buf;
			int cf = fscanf_s(m_fp, TEXT("%d %d %d %d %d %d %d %d %u %u %d %d %d %d-"),
				&rec->len, &rec->log_type, &rec->log_flags, &rec->nn, &rec->cb_app_path, &rec->cb_fn_name, &rec->cb_trace, &rec->tid, &rec->sec, &rec->msec, &rec->this_fn, &rec->call_site, &rec->fn_line, &rec->call_line);
            ss = (14 == cf);
            ss = ss && rec->isValid();
            if (rec->cbData())
                ss = ss && (1 == fread(rec->data, rec->cbData(), 1, m_fp));
            if (ss)
            {
                if (rec->isTrace())
                {
                    rec->data[rec->cbData()] = '\n';
                    rec->cb_trace++;
                    rec->len++;
                }
                rec->data[rec->cbData()] = 0;
            }

            ss = ss && rec->isValid();
            if (ss)
                appended = gArchive.append(rec);
            ss = ss && appended;
            if (feof(m_fp))
            {
                ss = true;
                break;
            }
            m_progress = ftell(m_fp);
        }
        if (!ss)
        {
            if (!appended)
                Helpers::ErrMessageBox(TEXT("Not enought memory to uppent record %d"), count);
            else
                Helpers::ErrMessageBox(TEXT("Record line %d corupted"), count);
        }
    }
}
#pragma warning(pop)
