/*
 * Copyright 2014, Rene Gollent, rene@gollent.com.
 * Distributed under the terms of the MIT License.
 */
#include "ExpressionEvaluationWindow.h"

#include <Alert.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>

#include "AutoLocker.h"

#include "MessageCodes.h"
#include "SourceLanguage.h"
#include "StackFrame.h"
#include "SyntheticPrimitiveType.h"
#include "Thread.h"
#include "UiUtils.h"
#include "UserInterface.h"
#include "Value.h"


enum {
	MSG_CHANGE_EVALUATION_TYPE = 'chet'
};


ExpressionEvaluationWindow::ExpressionEvaluationWindow(
	SourceLanguage* language, StackFrame* frame, ::Thread* thread,
	UserInterfaceListener* listener, BHandler* target)
	:
	BWindow(BRect(), "Evaluate Expression", B_FLOATING_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fLanguage(language),
	fExpressionInfo(NULL),
	fExpressionInput(NULL),
	fExpressionOutput(NULL),
	fEvaluateButton(NULL),
	fListener(listener),
	fStackFrame(frame),
	fThread(thread),
	fCloseTarget(target)
{
	fLanguage->AcquireReference();

	if (fStackFrame != NULL)
		fStackFrame->AcquireReference();

	if (fThread != NULL)
		fThread->AcquireReference();
}


ExpressionEvaluationWindow::~ExpressionEvaluationWindow()
{
	fLanguage->ReleaseReference();

	if (fStackFrame != NULL)
		fStackFrame->ReleaseReference();

	if (fThread != NULL)
		fThread->ReleaseReference();

	fExpressionInfo->ReleaseReference();
}


ExpressionEvaluationWindow*
ExpressionEvaluationWindow::Create(SourceLanguage* language, StackFrame* frame,
	::Thread* thread, UserInterfaceListener* listener, BHandler* target)
{
	ExpressionEvaluationWindow* self = new ExpressionEvaluationWindow(language,
		frame, thread, listener, target);

	try {
		self->_Init();
	} catch (...) {
		delete self;
		throw;
	}

	return self;

}


void
ExpressionEvaluationWindow::_Init()
{
	::Type* type = new SyntheticPrimitiveType(B_INT64_TYPE);
	BReference< ::Type> typeReference(type, true);
	fExpressionInfo = new ExpressionInfo(NULL, type);

	fExpressionInput = new BTextControl("Expression:", NULL,
		new BMessage(MSG_EVALUATE_EXPRESSION));
	BLayoutItem* labelItem = fExpressionInput->CreateLabelLayoutItem();
	BLayoutItem* inputItem = fExpressionInput->CreateTextViewLayoutItem();
	inputItem->SetExplicitMinSize(BSize(200.0, B_SIZE_UNSET));
	inputItem->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	labelItem->View()->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	fExpressionOutput = new BStringView("ExpressionOutputView", NULL);
	fExpressionOutput->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,
			B_SIZE_UNSET));

	BMenuField* typeField = new BMenuField("Type:", _BuildTypesMenu());
	typeField->Menu()->SetTargetForItems(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.AddGroup(B_HORIZONTAL, 4.0f)
			.Add(labelItem)
			.Add(inputItem)
			.Add(typeField->CreateLabelLayoutItem())
			.Add(typeField->CreateMenuBarLayoutItem())
		.End()
		.AddGroup(B_HORIZONTAL, 4.0f)
			.Add(new BStringView("OutputLabelView", "Result:"))
			.Add(fExpressionOutput)
		.End()
		.AddGroup(B_HORIZONTAL, 4.0f)
			.AddGlue()
			.Add((fEvaluateButton = new BButton("Evaluate",
					new BMessage(MSG_EVALUATE_EXPRESSION))))
		.End();

	fExpressionInput->SetTarget(this);
	fEvaluateButton->SetTarget(this);
	fExpressionInput->TextView()->MakeFocus(true);
}


BMenu*
ExpressionEvaluationWindow::_BuildTypesMenu()
{
	BMenu* menu = new BMenu("Types");
	menu->SetLabelFromMarked(true);

	_AddMenuItemForType(menu, B_INT8_TYPE);
	_AddMenuItemForType(menu, B_UINT8_TYPE);
	_AddMenuItemForType(menu, B_INT16_TYPE);
	_AddMenuItemForType(menu, B_UINT16_TYPE);
	_AddMenuItemForType(menu, B_INT32_TYPE);
	_AddMenuItemForType(menu, B_UINT32_TYPE);
	BMenuItem* item = _AddMenuItemForType(menu, B_INT64_TYPE);
	if (item != NULL)
		item->SetMarked(true);

	_AddMenuItemForType(menu, B_UINT64_TYPE);
	_AddMenuItemForType(menu, B_FLOAT_TYPE);
	_AddMenuItemForType(menu, B_DOUBLE_TYPE);

	return menu;
}


BMenuItem*
ExpressionEvaluationWindow::_AddMenuItemForType(BMenu* menu, type_code type)
{
	BMessage *message = new BMessage(MSG_CHANGE_EVALUATION_TYPE);
	message->AddInt32("type", type);

	BMenuItem* item = new BMenuItem(UiUtils::TypeCodeToString(type), message);
	menu->AddItem(item);

	return item;
}


void
ExpressionEvaluationWindow::ExpressionEvaluated(ExpressionInfo* info,
	status_t result, Value* value)
{
	BMessage message(MSG_EXPRESSION_EVALUATED);
	message.AddInt32("result", result);
	BReference<Value> reference;
	if (value != NULL) {
		message.AddPointer("value", value);
		reference.SetTo(value);
	}

	if (PostMessage(&message) == B_OK)
		reference.Detach();
}


void
ExpressionEvaluationWindow::Show()
{
	CenterOnScreen();
	BWindow::Show();
}


bool
ExpressionEvaluationWindow::QuitRequested()
{
	BMessenger messenger(fCloseTarget);
	messenger.SendMessage(MSG_EXPRESSION_WINDOW_CLOSED);

	return BWindow::QuitRequested();
}


void
ExpressionEvaluationWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_EVALUATE_EXPRESSION:
		{
			if (fExpressionInput->TextView()->TextLength() == 0)
				break;

			fExpressionInfo->SetExpression(fExpressionInput->Text());

			fListener->ExpressionEvaluationRequested(fLanguage,
				fExpressionInfo, fStackFrame, fThread);
			break;
		}

		case MSG_CHANGE_EVALUATION_TYPE:
		{
			uint32 typeConstant = message->FindInt32("type");
			PrimitiveType* type = dynamic_cast<PrimitiveType*>(
				fExpressionInfo->ResultType());
			if (type->TypeConstant() == typeConstant)
				break;

			type = new(std::nothrow) SyntheticPrimitiveType(
				typeConstant);
			if (type == NULL)
				break;

			BReference< ::Type> typeReference(type, true);
			fExpressionInfo->SetResultType(type);
			break;
		}

		case MSG_EXPRESSION_EVALUATED:
		{
			Value* value = NULL;
			BReference<Value> reference;
			if (message->FindPointer("value",
				reinterpret_cast<void**>(&value)) == B_OK) {
				reference.SetTo(value, true);
			}

			BString outputText;
			if (value != NULL) {
				BVariant variantValue;
				value->ToVariant(variantValue);
				if (variantValue.TypeIsInteger(variantValue.Type())) {
					value->ToString(outputText);
					outputText.SetToFormat("%#" B_PRIx64 " (%s)",
						variantValue.ToUInt64(), outputText.String());
				} else
					value->ToString(outputText);
			} else {
				status_t result;
				if (message->FindInt32("result", &result) != B_OK)
					result = B_ERROR;

				outputText.SetToFormat("Failed to evaluate expression: %s",
					strerror(result));
			}

			fExpressionOutput->SetText(outputText);
			break;
		}


		default:
			BWindow::MessageReceived(message);
			break;
	}

}