// Copyright � 2010-2015 The CefSharp Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

#include "Stdafx.h"
#include "Messages.h"
#include "../Serialization/Primitives.h"
#include "../Serialization/V8Serialization.h"
#include "EvaluateScriptDoneDelegate.h"
    
namespace CefSharp
{
    namespace Internals
    {
        using namespace Serialization;

        namespace Messaging
        {
            EvaluateScriptDoneDelegate::EvaluateScriptDoneDelegate(PendingTaskRepository<JavascriptResponse^>^ pendingTasks, Dictionary<int, IJavascriptCallbackFactory^>^ callbackFactories)
                :_pendingTasks(pendingTasks), _callbackFactories(callbackFactories)
            {

            }

            Task<JavascriptResponse^>^ EvaluateScriptDoneDelegate::EvaluateScriptAsync(CefRefPtr<CefBrowser> cefBrowser, int browserId, int frameId, String^ script, Nullable<TimeSpan> timeout)
            {
                TaskCompletionSource<JavascriptResponse^>^ completionSource = nullptr;
                //create a new taskcompletionsource based on wether we do or do not have a timeout
                auto callbackId = timeout.HasValue ? _pendingTasks->CreatePendingTaskWithTimeout(completionSource, timeout.Value) :
                    _pendingTasks->CreatePendingTask(completionSource);

                auto message = CefProcessMessage::Create(kEvaluateJavascript);
                auto argList = message->GetArgumentList();
                argList->SetInt(0, browserId);
                argList->SetInt(1, frameId);
                SetInt64(callbackId, argList, 2);
                argList->SetString(3, StringUtils::ToNative(script));

                cefBrowser->SendProcessMessage(CefProcessId::PID_RENDERER, message);

                return completionSource->Task;
            }

            bool EvaluateScriptDoneDelegate::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefProcessId source_process, CefRefPtr<CefProcessMessage> message)
            {
                auto handled = false;
                auto name = message->GetName();
                if (name == kEvaluateJavascriptDone)
                {
                    auto argList = message->GetArgumentList();
                    auto success = argList->GetBool(0);
                    auto callbackId = GetInt64(argList, 1);

                    IJavascriptCallbackFactory^ callbackFactory;
                    _callbackFactories->TryGetValue(browser->GetIdentifier(), callbackFactory);

                    FinishTask(callbackId, success, argList, callbackFactory);

                    handled = true;
                }

                return handled;
            }

            void EvaluateScriptDoneDelegate::FinishTask(int64 callbackId, bool success, CefRefPtr<CefListValue> message, IJavascriptCallbackFactory^ callbackFactory)
            {
                auto pendingTask = _pendingTasks->RemovePendingTask(callbackId);
                if (pendingTask != nullptr)
                {
                    pendingTask->SetResult(CreateResponse(success, message, callbackFactory));
                }
            }

            JavascriptResponse^ EvaluateScriptDoneDelegate::CreateResponse(bool success, CefRefPtr<CefListValue> message, IJavascriptCallbackFactory^ callbackFactory)
            {
                auto result = gcnew JavascriptResponse();
                result->Success = success;

                if (success)
                {
                    result->Result = DeserializeV8Object(message, 2, callbackFactory);
                }
                else
                {
                    result->Message = StringUtils::ToClr(message->GetString(2));
                }

                return result;
            }
        }
    }
}