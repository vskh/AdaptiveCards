// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "pch.h"
#include "InputValue.h"
#include "json/json.h"
#include "XamlHelpers.h"
#include <windows.globalization.datetimeformatting.h>

using namespace AdaptiveCards::Rendering::Uwp;

namespace winrt::AdaptiveCards::Rendering::Uwp
{
    // TODO: what is this function?
    void ValidateIfNeeded(winrt::IAdaptiveInputValue const& inputValue)
    {
        auto currentValue = inputValue.CurrentValue();

        inputValue.Validate();
    }

    InputValue::InputValue(winrt::IAdaptiveInputElement const& adaptiveInputElement,
                           winrt::UIElement const& uiInputElement,
                           winrt::Border const& validationBorder) :
        m_adaptiveInputElement(adaptiveInputElement),
        m_uiInputElement(uiInputElement), m_validationBorder(validationBorder), m_validationError(nullptr)

    {
    }

    InputValue::InputValue() : m_uiInputElement(nullptr), m_validationError(nullptr){};

    bool InputValue::Validate()
    {
        auto isValid = IsValueValid();
        SetValidation(isValid);

        return isValid;
    }

    // TODO: should we return a bool?
    void InputValue::SetFocus()
    {
        if (const auto inputAsControl = m_uiInputElement.try_as<winrt::Control>())
        {
            inputAsControl.Focus(winrt::FocusState::Programmatic);
        }
    }

    void InputValue::SetAccessibilityProperties(bool isInputValid)
    {
        // This smart pointer is created as the variable inputUIElementParentContainer may contain the border instead of the
        // actual element if validations are required. If these properties are set into the border then they are not mentioned.
        auto inputUIElementAsDependencyObject = m_uiInputElement.as<winrt::DependencyObject>();

        auto uiElementDescribers = winrt::AutomationProperties::GetDescribedBy(inputUIElementAsDependencyObject);

        auto uiValidationErrorAsDependencyObject = m_validationError.as<winrt::DependencyObject>();

        uint32_t index;
        bool found = uiElementDescribers.IndexOf(uiValidationErrorAsDependencyObject, index);

        // If the error message is visible then the input element must be described by it, otherwise we try to remove it from the list of describers
        if (!isInputValid && !found)
        {
            uiElementDescribers.Append(uiValidationErrorAsDependencyObject);
        }
        else if (isInputValid && found)
        {
            uiElementDescribers.RemoveAt(index);
        }

        winrt::AutomationProperties::SetIsDataValidForForm(inputUIElementAsDependencyObject, isInputValid);
    }

    bool InputValue::IsValueValid()
    {
        auto isRequired = m_adaptiveInputElement.IsRequired();

        bool isRequiredValid = true;
        if (isRequired)
        {
            auto currentValue = CurrentValue();

            isRequiredValid = !currentValue.empty();
        }

        return isRequiredValid;
    }

    void InputValue::SetValidation(bool isInputValid)
    {
        // Show or hide the border
        if (m_validationBorder)
        {
            if (isInputValid)
            {
                m_validationBorder.BorderThickness({0, 0, 0, 0});
            }
            else
            {
                m_validationBorder.BorderThickness({1, 1, 1, 1});
            }
        }

        // Show or hide the error message
        if (m_validationError)
        {
            if (isInputValid)
            {
                m_validationError.Visibility(winrt::Visibility::Collapsed);
            }
            else
            {
                m_validationError.Visibility(winrt::Visibility::Visible);
            }

            SetAccessibilityProperties(isInputValid);
        }
    }

    TextInputBase::TextInputBase(winrt::AdaptiveTextInput const& adaptiveTextInput,
                                 winrt::UIElement const& uiTextInputElement,
                                 winrt::Border const& validationBorder) :
        // TODO: is this the proper way for calling base ctor? Do I need to cast the first argument?
        InputValue(adaptiveTextInput, uiTextInputElement, validationBorder),
        m_adaptiveTextInput(adaptiveTextInput)
    {
    }

    bool TextInputBase::IsValueValid()
    {
        // Call the base class to validate isRequired

        if (!InputValue::IsValueValid())
        {
            return false;
        }

        // Validate the regex if one exists
        auto regex = m_adaptiveTextInput.Regex();
        auto currentValue = CurrentValue();

        bool isRegexValid = true;
        if (!regex.empty() && !currentValue.empty())
        {
            std::string stringPattern = HStringToUTF8(regex);
            std::regex pattern(stringPattern);

            std::string currentValueStdString = HStringToUTF8(currentValue);

            std::smatch matches;
            isRegexValid = std::regex_match(currentValueStdString, matches, pattern);
        }

        return isRegexValid;
    }

    TextInputValue::TextInputValue(winrt::AdaptiveTextInput const& adaptiveTextInput,
                                   winrt::TextBox const& uiTextBoxElement,
                                   winrt::Border const& validationBorder) :
        TextInputBase(adaptiveTextInput, uiTextBoxElement, validationBorder),
        m_textBoxElement(uiTextBoxElement)
    {
    }

    PasswordInputValue::PasswordInputValue(winrt::AdaptiveTextInput const& adaptiveTextInput,
                                           winrt::PasswordBox const& uiPasswordElement,
                                           winrt::Border const& validationBorder) :
        TextInputBase(adaptiveTextInput, uiPasswordElement, validationBorder),
        m_passwordElement(uiPasswordElement)
    {
    }

    NumberInputValue::NumberInputValue(winrt::AdaptiveNumberInput const& adaptiveNumberInput,
                                       winrt::TextBox const& uiInputTextBoxElement,
                                       winrt::Border const& validationBorder) :
        InputValue(adaptiveNumberInput, uiInputTextBoxElement, validationBorder),
        m_adaptiveNumberInput(adaptiveNumberInput), m_textBoxElement(uiInputTextBoxElement)
    {
    }

    bool NumberInputValue::IsValueValid()
    {
        // Call the base class to validate isRequired
        if (!InputValue::IsValueValid())
        {
            return false;
        }

        auto max = m_adaptiveNumberInput.Max();
        auto min = m_adaptiveNumberInput.Min();
        auto currentValue = CurrentValue();
        bool isValid = true;

        // If there is a value, confirm that it's a number and within the min/max range
        if (!currentValue.empty())
        {
            try
            {
                const std::string currentValueStdString = HStringToUTF8(currentValue);
                double currentDouble = std::stod(currentValueStdString);

                if (max)
                {
                    double maxDouble = max.Value();
                    isValid = (currentDouble <= maxDouble);
                }

                if (min)
                {
                    double minDouble = min.Value();
                    isValid = (currentDouble >= minDouble);
                }
            }
            catch (...)
            {
                // If stoi failed this isn't a valid number
                isValid = false;
            }
        }

        return isValid;
    }

    DateInputValue::DateInputValue(winrt::AdaptiveDateInput const& adaptiveDateInput,
                                   winrt::CalendarDatePicker const& uiDatePickerElement,
                                   winrt::Border const& validationBorder) :
        InputValue(adaptiveDateInput, uiDatePickerElement, validationBorder),
        m_adaptiveDateInput(adaptiveDateInput), m_datePickerElement(uiDatePickerElement)
    {
    }

    winrt::hstring DateInputValue::CurrentValue()
    {
        auto dateRef = m_datePickerElement.Date();

        winrt::hstring formattedDate{};
        if (dateRef)
        {
            auto date = dateRef.Value();

            // TODO: is this correct?
            winrt::DateTimeFormatter dateTimeFormatter{L"{year.full}-{month.integer(2)}-{day.integer(2)}"};
            formattedDate = dateTimeFormatter.Format(date);
        }

        return formattedDate;
    }

    TimeInputValue::TimeInputValue(winrt::AdaptiveTimeInput adaptiveTimeInput,
                                   winrt::TimePicker uiTimePickerElement,
                                   winrt::Border validationBorder) :
        InputValue(adaptiveTimeInput, uiTimePickerElement, validationBorder),
        m_adaptiveTimeInput(adaptiveTimeInput), m_timePickerElement(uiTimePickerElement)
    {
    }

    winrt::hstring TimeInputValue::CurrentValue()
    {
        auto timeSpanReference = m_timePickerElement.SelectedTime();

        char buffer[6] = {0};

        if (timeSpanReference)
        {
            // timeSpan here is std::chrono::duration<int64_t, 100 * std::nano>, so count will give the same value as duration before
            auto timeSpan = timeSpanReference.Value();

            // TODO: Is is the same number?
            uint64_t totalMinutes = timeSpan.count() / 10000000 / 60;
            uint64_t hours = totalMinutes / 60;
            uint64_t minutesPastTheHour = totalMinutes - (hours * 60);

            sprintf_s(buffer, sizeof(buffer), "%02llu:%02llu", hours, minutesPastTheHour);
        }

        return UTF8ToHString(std::string(buffer));
    }

    bool TimeInputValue::IsValueValid()
    {
        // Call the base class to validate isRequired
        if (!InputValue::IsValueValid())
        {
            return false;
        }

        // If time is set, validate max and min
        bool isMaxMinValid = true;
        auto timeSpanReference = m_timePickerElement.SelectedTime();

        if (timeSpanReference)
        {
            auto currentTime = timeSpanReference.Value();

            auto minTimeString = m_adaptiveTimeInput.Min();
            if (!minTimeString.empty())
            {
                std::string minTimeStdString = HStringToUTF8(minTimeString);
                unsigned int minHours, minMinutes;
                if (::AdaptiveCards::DateTimePreparser::TryParseSimpleTime(minTimeStdString, minHours, minMinutes))
                {
                    winrt::TimeSpan minTime{(int64_t)(minHours * 60 + minMinutes) * 1000000 * 60};
                    isMaxMinValid &= currentTime.count() >= minTime.count();
                }
            }

            auto maxTimeString = m_adaptiveTimeInput.Max();
            if (!maxTimeString.empty())
            {
                std::string maxTimeStdString = HStringToUTF8(maxTimeString);
                unsigned int maxHours, maxMinutes;
                if (::AdaptiveCards::DateTimePreparser::TryParseSimpleTime(maxTimeStdString, maxHours, maxMinutes))
                {
                    winrt::TimeSpan maxTime{(int64_t)(maxHours * 60 + maxMinutes) * 10000000 * 60};
                    isMaxMinValid &= currentTime.count() <= maxTime.count();
                }
            }
        }
        return isMaxMinValid;
    }

    ToggleInputValue::ToggleInputValue(winrt::AdaptiveToggleInput adaptiveToggleInput,
                                       winrt::CheckBox uiCheckBoxElement,
                                       winrt::Border validationBorder) :
        InputValue(adaptiveToggleInput, uiCheckBoxElement, validationBorder),
        m_adaptiveToggleInput(adaptiveToggleInput), m_checkBoxElement(uiCheckBoxElement)
    {
    }

    winrt::hstring ToggleInputValue::CurrentValue()
    {
        auto checkedValue = ::AdaptiveCards::Rendering::Uwp::XamlHelpers::GetToggleValue(m_checkBoxElement);

        if (checkedValue)
        {
            return m_adaptiveToggleInput.ValueOn();
        }
        else
        {
            return m_adaptiveToggleInput.ValueOff();
        }
    }

    bool ToggleInputValue::IsValueValid()
    {
        // Don't use the base class IsValueValid to validate required for toggle. That method counts required as
        // satisfied if any value is set, but for toggle required means the check box is checked. An unchecked value
        // will still have a value (either false, or whatever's in valueOff).
        auto isRequired = m_adaptiveInputElement.IsRequired();

        bool meetsRequirement = true;
        if (isRequired)
        {
            // MeetsRequirement is true if toggle is checked
            meetsRequirement = ::AdaptiveCards::Rendering::Uwp::XamlHelpers::GetToggleValue(m_checkBoxElement);
        }
        return meetsRequirement;
    }

    std::string GetChoiceValue(winrt::AdaptiveChoiceSetInput const& choiceInput, uint32_t selectedIndex)
    {
        if (selectedIndex != -1)
        {
            auto choices = choiceInput.Choices();

            // TODO: should we check if selectedIndex is a valid index? And if not, should we log an error / throw an exception?
            auto choice = choices.GetAt(selectedIndex);
            return HStringToUTF8(choice.Value());
        }
        // TODO: what do we do here? throw?
        return "";
    }

    CompactChoiceSetInputValue::CompactChoiceSetInputValue(winrt::AdaptiveChoiceSetInput adaptiveChoiceSetInput,
                                                           winrt::Selector choiceSetSelector,
                                                           winrt::Border validationBorder) :
        InputValue(adaptiveChoiceSetInput, choiceSetSelector, validationBorder),
        m_adaptiveChoiceSetInput(adaptiveChoiceSetInput), m_selectorElement(choiceSetSelector)
    {
    }

    winrt::hstring CompactChoiceSetInputValue::CurrentValue()
    {
        auto choiceSetStyle = m_adaptiveChoiceSetInput.ChoiceSetStyle();
        auto isMultiSelect = m_adaptiveChoiceSetInput.IsMultiSelect();
        auto selectedIndex = m_selectorElement.SelectedIndex();

        std::string choiceValue = GetChoiceValue(m_adaptiveChoiceSetInput, selectedIndex);
        return UTF8ToHString(choiceValue);
    }

    ExpandedChoiceSetInputValue::ExpandedChoiceSetInputValue(winrt::AdaptiveChoiceSetInput adaptiveChoiceSetInput,
                                                             winrt::Panel choiceSetPanelElement,
                                                             winrt::Border validationBorder) :
        InputValue(adaptiveChoiceSetInput, choiceSetPanelElement, validationBorder),
        m_adaptiveChoiceSetInput(adaptiveChoiceSetInput), m_panelElement(choiceSetPanelElement)
    {
    }

    winrt::hstring ExpandedChoiceSetInputValue::CurrentValue()
    {
        // Get the panel children
        auto panelChildren = m_panelElement.Children();
        auto isMultiSelect = m_adaptiveChoiceSetInput.IsMultiSelect();
        uint32_t index = 0;

        if (isMultiSelect)
        {
            // For multiselect, gather all the inputs in a comma delimited list
            std::string multiSelectValues;
            for (auto element : panelChildren)
            {
                if (::AdaptiveCards::Rendering::Uwp::XamlHelpers::GetToggleValue(element))
                {
                    multiSelectValues += GetChoiceValue(m_adaptiveChoiceSetInput, index) + ",";
                }
                index++;
            }

            if (!multiSelectValues.empty())
            {
                multiSelectValues = multiSelectValues.substr(0, (multiSelectValues.size() - 1));
            }
            return UTF8ToHString(multiSelectValues);
        }
        else
        {
            // Look for the single selected choice
            uint32_t selectedIndex = -1;
            for (auto element : panelChildren)
            {
                if (::AdaptiveCards::Rendering::Uwp::XamlHelpers::GetToggleValue(element))
                {
                    selectedIndex = index;
                    break;
                }
                index++;
            }

            // TODO: Bad readability right?
            return UTF8ToHString(GetChoiceValue(m_adaptiveChoiceSetInput, selectedIndex));
        }
    }

    void ExpandedChoiceSetInputValue::SetFocus()
    {
        // Put focus on the first choice in the choice set
        auto panelChildren = m_panelElement.Children();
        auto firstChoice = panelChildren.GetAt(0);

        if (const auto choiceAsControl = firstChoice.try_as<winrt::Control>())
        {
            // TODO: do we need to return bool indicating whether focus was set?
            choiceAsControl.Focus(winrt::FocusState::Programmatic);
        }
    }

    FilteredChoiceSetInputValue::FilteredChoiceSetInputValue(winrt::AdaptiveChoiceSetInput adaptiveChoiceSetInput,
                                                             winrt::AutoSuggestBox autoSuggestBox,
                                                             winrt::Border validationBorder) :
        InputValue(adaptiveChoiceSetInput, autoSuggestBox, validationBorder),
        m_adaptiveChoiceSetInput(adaptiveChoiceSetInput), m_autoSuggestBox(autoSuggestBox)
    {
    }

    winrt::hstring FilteredChoiceSetInputValue::CurrentValue()
    {
        auto choices = m_adaptiveChoiceSetInput.Choices();
        auto selectedChoice = GetSelectedChoice();
        if (selectedChoice)

        {
            return selectedChoice.Value();
        }

        // TODO: is this correct to return empty hstring? What do we do here?
        return L"";
    }

    bool FilteredChoiceSetInputValue::IsValueValid()
    {
        bool isValid = true;

        // Check if there's text in the autoSuggestBox
        auto textHString = m_autoSuggestBox.Text();

        if (textHString.empty())
        {
            // Empty input is only valid if it's not required
            auto isRequired = m_adaptiveInputElement.IsRequired();
            isValid = !isRequired;
        }
        else
        {
            // Non-empty input must match one of the exisiting choices
            auto selectedChoice = GetSelectedChoice();
            isValid = (selectedChoice != nullptr);
        }

        return isValid;
    }

    winrt::AdaptiveChoiceInput FilteredChoiceSetInputValue::GetSelectedChoice()
    {
        auto textHString = m_autoSuggestBox.Text();
        std::string text = HStringToUTF8(textHString);
        auto choices = m_adaptiveChoiceSetInput.Choices();

        winrt::AdaptiveChoiceInput selectedChoice{nullptr};
        for (auto choice : choices)
        {
            auto titleHString = choice.Title();

            std::string title = HStringToUTF8(titleHString);

            if (!::AdaptiveCards::ParseUtil::ToLowercase(text).compare(::AdaptiveCards::ParseUtil::ToLowercase(title)))
            {
                selectedChoice = choice;
                // TODO: do we need to break here? can we just return? why do we need to keep going
            }
        }

        return selectedChoice;
    }
}
