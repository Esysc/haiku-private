/*
 * Copyright 2015, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */


#include "SettingsParser.h"

#include <DriverSettingsMessageAdapter.h>


class ConditionConverter : public DriverSettingsConverter {
public:
	status_t ConvertFromDriverSettings(const driver_parameter& parameter,
		const char* name, int32 index, uint32 type, BMessage& target)
	{
		BMessage message;
		if (strcmp(parameter.name, "if") == 0) {
			// Parse values directly following "if", with special
			// handling for the "not" operator.
			if (index != 0)
				return B_OK;

			BMessage* add = &target;
			bool not = parameter.value_count > 1
				&& strcmp(parameter.values[0], "not") == 0;
			if (not) {
				add = &message;
				index++;
			}

			status_t status = _AddSubMessage(parameter, index, *add);
			if (status == B_OK && not)
				status = target.AddMessage("not", &message);

			return status;
		}
		if (strcmp(parameter.name, "not") == 0) {
			if (index != 0)
				return B_OK;

			return _AddSubMessage(parameter, index, target);
		}

		message.AddString("args", parameter.values[index]);
		return target.AddMessage(parameter.name, &message);
	}

	status_t ConvertEmptyFromDriverSettings(
		const driver_parameter& parameter, const char* name, uint32 type,
		BMessage& target)
	{
		if (parameter.parameter_count != 0)
			return B_OK;

		BMessage message;
		return target.AddMessage(name, &message);
	}

private:
	status_t _AddSubMessage(const driver_parameter& parameter, int32 index,
		BMessage& target)
	{
		const char* condition = parameter.values[index];
		BMessage args;
		for (index++; index < parameter.value_count; index++) {
			status_t status = args.AddString("args",
				parameter.values[index]);
			if (status != B_OK)
				return status;
		}
		return target.AddMessage(condition, &args);
	}
};


const static settings_template kConditionTemplate[] = {
	{B_STRING_TYPE, NULL, NULL, true, new ConditionConverter()},
	{B_MESSAGE_TYPE, "not", kConditionTemplate},
	{B_MESSAGE_TYPE, "and", kConditionTemplate},
	{B_MESSAGE_TYPE, "or", kConditionTemplate},
	{0, NULL, NULL}
};

const static settings_template kPortTemplate[] = {
	{B_STRING_TYPE, "name", NULL, true},
	{B_INT32_TYPE, "capacity", NULL},
};

const static settings_template kJobTemplate[] = {
	{B_STRING_TYPE, "name", NULL, true},
	{B_BOOL_TYPE, "disabled", NULL},
	{B_STRING_TYPE, "launch", NULL},
	{B_STRING_TYPE, "requires", NULL},
	{B_BOOL_TYPE, "legacy", NULL},
	{B_MESSAGE_TYPE, "port", kPortTemplate},
	{B_MESSAGE_TYPE, "if", kConditionTemplate},
	{B_BOOL_TYPE, "no_safemode", NULL},
	{0, NULL, NULL}
};

const static settings_template kTargetTemplate[] = {
	{B_STRING_TYPE, "name", NULL, true},
	{B_BOOL_TYPE, "reset", NULL},
	{B_MESSAGE_TYPE, "if", kConditionTemplate},
	{B_MESSAGE_TYPE, "job", kJobTemplate},
	{B_MESSAGE_TYPE, "service", kJobTemplate},
	{0, NULL, NULL}
};

const static settings_template kSettingsTemplate[] = {
	{B_MESSAGE_TYPE, "target", kTargetTemplate},
	{B_MESSAGE_TYPE, "job", kJobTemplate},
	{B_MESSAGE_TYPE, "service", kJobTemplate},
	{0, NULL, NULL}
};


SettingsParser::SettingsParser()
{
}


status_t
SettingsParser::ParseFile(const char* path, BMessage& settings)
{
	DriverSettingsMessageAdapter adapter;
	return adapter.ConvertFromDriverSettings(path, kSettingsTemplate, settings);
}


#ifdef TEST_HAIKU


status_t
SettingsParser::Parse(const char* text, BMessage& settings)
{
	void* driverSettings = parse_driver_settings_string(text);
	if (driverSettings == NULL)
		return B_BAD_VALUE;

	DriverSettingsMessageAdapter adapter;
	status_t status = adapter.ConvertFromDriverSettings(
		*get_driver_settings(driverSettings), kSettingsTemplate, settings);

	delete_driver_settings(driverSettings);
	return status;
}


#endif	// TEST_HAIKU