/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SQLTransactionClient.h"

#include "ChromeClient.h"
#include "Database.h"
#include "DatabaseThread.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "OriginQuotaManager.h"
#include "Page.h"
#include "SQLTransaction.h"

namespace WebCore {

void SQLTransactionClient::didCommitTransaction(SQLTransaction* transaction)
{
    ASSERT(currentThread() == transaction->database()->document()->databaseThread()->getThreadID());
    Database* database = transaction->database();
    DatabaseTracker::tracker().scheduleNotifyDatabaseChanged(
        database->document()->securityOrigin(), database->stringIdentifier());
}

void SQLTransactionClient::didExecuteStatement(SQLTransaction* transaction)
{
    ASSERT(currentThread() == transaction->database()->document()->databaseThread()->getThreadID());
    OriginQuotaManager& manager(DatabaseTracker::tracker().originQuotaManager());
    Locker<OriginQuotaManager> locker(manager);
    manager.markDatabase(transaction->database());
}

bool SQLTransactionClient::didExceedQuota(SQLTransaction* transaction)
{
    ASSERT(isMainThread());
    Database* database = transaction->database();
    Page* page = database->document()->page();
    ASSERT(page);

    RefPtr<SecurityOrigin> origin = database->securityOriginCopy();

    unsigned long long currentQuota = DatabaseTracker::tracker().quotaForOrigin(origin.get());
    page->chrome()->client()->exceededDatabaseQuota(database->document()->frame(), database->stringIdentifier());
    unsigned long long newQuota = DatabaseTracker::tracker().quotaForOrigin(origin.get());
    return (newQuota > currentQuota);
}

}
