export const InputTypes = {
    TextField: "TextField",
    Switch: "Switch",
    Select: "Select"
}

// Generates the initial state (config output) for a given set of field setups
export function generateInitialState(fieldsSetup) {
    const config = {};

    for(const field of fieldsSetup) {
        if (field.dataType === "object") {
            config[field.name] = field.objectType ? generateInitialState(field.objectType) : {};
        } else if(field.dataType === "array") {
            config[field.name] = [];
        }
        else {
            if (field.default || field.default == 0) {
                config[field.name] = field.default;
            } else if (field.dataType === "boolean") {
                config[field.name] = false;
            } else if (field.inputType == InputTypes.TextField) {
                config[field.name] = '';
            }
        }
    }

    return config;
}

// For arrays that don't contain objects, this method generates the field setup
// to populate in the form
export function fieldConfigForArrayItem(fieldSetup) {
    const arrayField = {...fieldSetup};
    arrayField.dataType = arrayField.arrayType;
    delete arrayField.arrayType;
    return arrayField;
}