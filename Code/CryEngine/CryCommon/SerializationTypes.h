// this file is included anywhere that we need a complete list of primitive serializable types

SERIALIZATION_TYPE(bool)
SERIALIZATION_TYPE(float)
SERIALIZATION_TYPE(Vec2)
SERIALIZATION_TYPE(Vec3)
SERIALIZATION_TYPE(Quat)
SERIALIZATION_TYPE(Ang3)
SERIALIZATION_TYPE(int8)
SERIALIZATION_TYPE(int16)
SERIALIZATION_TYPE(int32)
SERIALIZATION_TYPE(int64)
SERIALIZATION_TYPE(uint8)
SERIALIZATION_TYPE(uint16)
SERIALIZATION_TYPE(uint32)
SERIALIZATION_TYPE(uint64)
SERIALIZATION_TYPE(ScriptAnyValue) // not for network - only for save games
SERIALIZATION_TYPE(CTimeValue)
SERIALIZATION_TYPE(SNetObjectID)
SERIALIZATION_TYPE(XmlNodeRef) // not for network - only for save games
