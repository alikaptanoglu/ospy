//
// Copyright (c) 2007 Ole Andr� Vadla Ravn�s <oleavr@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "stdafx.h"
#include "BinaryLogger.h"
#include "Agent.h"

namespace oSpy {

typedef struct {
    SLIST_ENTRY entry;
    Logging::Event *ev;
} PendingEvent;

BinaryLogger::BinaryLogger(Agent *agent, const OWString &filename)
    : m_agent(agent)
{
    m_handle = CreateFileW(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_handle == INVALID_HANDLE_VALUE)
        throw runtime_error("CreateFile failed");

    m_destroyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    InitializeSListHead(&m_pendingEvents);

    m_loggingThreadHandle = CreateThread(NULL, 0, LoggingThreadFuncWrapper, this, 0, NULL);
}

BinaryLogger::~BinaryLogger()
{
    SetEvent(m_destroyEvent);
    FlushPending();
    CloseHandle(m_handle);
}

Logging::Event *
BinaryLogger::NewEvent(const OString &eventType)
{
    return new Logging::Event(this, m_agent->GetNextLogIndex(), eventType);
}

void
BinaryLogger::SubmitEvent(Logging::Event *ev)
{
    if (WaitForSingleObject(m_destroyEvent, 0) != WAIT_OBJECT_0)
    {
        PendingEvent *pe = new PendingEvent;
        pe->ev = ev;
        InterlockedPushEntrySList(&m_pendingEvents, &pe->entry);
    }
    else
    {
        delete ev;
    }
}

void
BinaryLogger::FlushPending()
{
    PendingEvent *pe;
    while ((pe = reinterpret_cast<PendingEvent *>(InterlockedFlushSList(&m_pendingEvents))) != NULL)
    {
        PendingEvent *cur = pe;

        do
        {
            BinarySerializer serializer;
            serializer.AppendNode(cur->ev);

            PendingEvent *next = reinterpret_cast<PendingEvent *>(cur->entry.Next);
            delete cur->ev;
            delete cur;

            const OString &buf = serializer.GetData();

            DWORD bytesWritten;
            if (!WriteFile(m_handle, buf.data(), static_cast<DWORD>(buf.size()), &bytesWritten, NULL))
                throw Error("WriteFile failed");

            if (bytesWritten != buf.size())
                throw Error("short write");

            m_agent->AddBytesLogged(static_cast<LONG>(buf.size()));

            cur = next;
        }
        while (cur != NULL);
    }
}

DWORD WINAPI
BinaryLogger::LoggingThreadFuncWrapper(LPVOID param)
{
    BinaryLogger *instance = reinterpret_cast<BinaryLogger *>(param);
    instance->LoggingThreadFunc();
    return 0;
}

void
BinaryLogger::LoggingThreadFunc()
{
    ReentranceProtector protector;

    while (WaitForSingleObject(m_destroyEvent, 5000) != WAIT_OBJECT_0)
    {
        FlushPending();
    }
}

void
BinarySerializer::AppendNode(Logging::Node *node)
{
    // Name
    AppendString(node->GetName());

    // Fields
    {
        AppendDWord(node->GetFieldCount());
        Logging::Node::FieldListConstIter iter, endIter = node->FieldsIterEnd();

        for (iter = node->FieldsIterBegin(); iter != endIter; iter++)
        {
            AppendString(iter->first);
            AppendString(iter->second);
        }
    }

    // Content
    AppendDWord(node->GetContentIsRaw());
    AppendString(node->GetContent());

    // Children
    {
        AppendDWord(node->GetChildCount());
        Logging::Node::ChildListConstIter iter, endIter = node->ChildrenIterEnd();

        for (iter = node->ChildrenIterBegin(); iter != endIter; iter++)
        {
            AppendNode(*iter);
        }
    }
}

void
BinarySerializer::AppendString(const OString &s)
{
    AppendDWord(static_cast<DWORD>(s.size()));
    if (s.size() > 0)
        m_buf.append(s.data(), s.size());
}

void
BinarySerializer::AppendDWord(DWORD dw)
{
    m_buf.append(reinterpret_cast<const char *>(&dw), sizeof(dw));
}

} // namespace oSpy
