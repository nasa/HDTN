import { InputTypes } from "./utils";
import ConfigTextField from "./components/text_field";
import ConfigSwitch from "./components/switch";
import ConfigSelect from "./components/select";
import ConfigArrayField from "./array_field";

export default function ConfigField(props) {
    const {fieldSetup, target, handleAdd, handleChange, handleDelete, refreshState} = props;
    
    if (fieldSetup.dataType === "array") {
        return (<ConfigArrayField fieldSetup={fieldSetup} target={target} handleAdd={handleAdd} handleChange={handleChange} handleDelete={handleDelete} refreshState={refreshState}></ConfigArrayField>)
    }

    if (fieldSetup.inputType == InputTypes.TextField) {
        return (<ConfigTextField fieldSetup={fieldSetup} value={target[fieldSetup.name]} handleChange={handleChange}></ConfigTextField>);
    } else if (fieldSetup.inputType == InputTypes.Switch) {
        return (<ConfigSwitch fieldSetup={fieldSetup} value={target[fieldSetup.name]} handleChange={handleChange}></ConfigSwitch>);
    } else if (fieldSetup.inputType == InputTypes.Select) {
        return (<ConfigSelect fieldSetup={fieldSetup} value={target[fieldSetup.name]} handleChange={handleChange}></ConfigSelect>);
    }
}