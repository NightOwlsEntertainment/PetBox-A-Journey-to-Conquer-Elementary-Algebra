#pragma once

namespace Serialization
{

inline bool Serialize(Serialization::IArchive& ar, Serialization::OutputFilePath& value, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(Serialization::SStruct::forEdit(value), name, label);
	else
		return ar(*value.path, name, label);
}

}
