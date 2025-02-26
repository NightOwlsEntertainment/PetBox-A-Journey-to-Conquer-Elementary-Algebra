// Copyright 2001-2015 Crytek GmbH. All rights reserved.

#ifndef IDYNAMICREPONSESYSTEM_H_INCLUDED
#define IDYNAMICREPONSESYSTEM_H_INCLUDED

#include <Serialization/Forward.h>
#include "HashedString.h"

namespace DRS
{
	//we dont provide an interface for this, because we do not want these classes to have a vtable.
	class CResponseVariable;
	
	struct IResponseAction;
	typedef Serialization::ClassFactory<IResponseAction> ActionSerializationClassFactory; 
	typedef std::shared_ptr<IResponseAction> IResponseActionSharedPtr;

	struct IResponseCondition;
	typedef Serialization::ClassFactory<IResponseCondition> ConditionSerializationClassFactory;
	typedef std::shared_ptr<IResponseCondition> IConditionSharedPtr;

	struct IDataImportHelper
	{
		typedef IConditionSharedPtr (*CondtionCreatorFct)(const string&, const char* szFormatName);
		typedef IResponseActionSharedPtr (*ActionCreatorFct)(const string&, const char* szFormatName);

		typedef uint32 ResponseID;
		typedef uint32 ResponseSegmentID;
		static const uint32 INVALID_ID = -1;

		virtual ResponseID AddSignalResponse(const string& szName) = 0;
		virtual bool AddResponseCondition(ResponseID responseID, IConditionSharedPtr pCondition, bool bNegated) = 0;
		virtual bool AddResponseAction(ResponseSegmentID segmentID, IResponseActionSharedPtr pAction) = 0;
		virtual ResponseSegmentID AddResponseSegement(ResponseID parentResponse, const string& szName) = 0;
		virtual bool AddResponseSegementAction(ResponseID parentResponse, ResponseSegmentID segmentID, IResponseActionSharedPtr pAction) = 0;
		virtual bool AddResponseSegementPostAction(ResponseID parentResponse, ResponseSegmentID segmentID, IResponseActionSharedPtr pAction) = 0;
		virtual bool AddResponseSegementCondition(ResponseID parentResponse, ResponseSegmentID segmentID, IConditionSharedPtr pConditions, bool bNegated = false) = 0;

		virtual bool HasActionCreatorForType(const CHashedString& type) = 0;
		virtual bool HasConditionCreatorForType(const CHashedString& type) = 0;
		virtual IResponseActionSharedPtr CreateActionFromString(const CHashedString& type, const string& data, const char* szFormatName) = 0;
		virtual IConditionSharedPtr CreateConditionFromString(const CHashedString& type, const string& data, const char* szFormatName) = 0;
		virtual void RegisterConditionCreator(const CHashedString& conditionType, CondtionCreatorFct pFunc) = 0;
		virtual void RegisterActionCreator(const CHashedString& actionTyp, ActionCreatorFct pFunc) = 0;

		virtual void Reset() = 0;
		virtual void Serialize(Serialization::IArchive& ar) = 0;
	};

	struct IVariableCollection
	{
		virtual ~IVariableCollection() {}

		// <title CreateVariable>
		// Syntax: CreateVariable("VariableName", int/float/string InitialValue)
		// Description:
		//      Creates a new variable in this VariableCollection and sets it to the specified initial value.
		//      REMARK: Creating variables with names that already exist will cause undefined behavior. 
		// Arguments:
		//      name - unique name of the new variable to be generated. Must not exist in the collection before
		//      initial value - the initial value (of type int, float or const char*) of the newly generated variable. the string pointer can be released after calling this function.
		// Returns:
		//      Returns a pointer to the newly generated variable. The actual type of the variable depends on the implementation of the DRS. Could be a class or just an identifier.
		// See Also:
		//      SetVariableValue, GetVariable
		virtual CResponseVariable* CreateVariable(const CHashedString& name, int initialValue) = 0;
		virtual CResponseVariable* CreateVariable(const CHashedString& name, float initialValue) = 0;
		virtual CResponseVariable* CreateVariable(const CHashedString& name, const char* initialValue) = 0;

		// <title SetVariableValue (by Variablename)>
		// Syntax: SetVariableValue("Health", 0.5f, true, 0.0f)
		// Description:
		//      Fetches the variable with the given name. Optionally (if the third parameter is set to true) will create the Variable if not existing. Will then change the value of the variable to the given value.
		//		The value-type of the variable and the value-type of the parameter needs to fit. Otherwise the behavior is undefined.
		//      HINT: If you going to change the value of this variable a lot, consider storing a pointer to the variable and use the SetVariableValue which takes a variable-pointer instead. It avoids the cost of the search.
		// Arguments:
		//      name - the name of the variable to set. If the variable is not existing and createIfNotExisting is true, it will be created.
		//      newValue - the value to set. The type of the newValue must match the previous type of the variable
		//		createIfNotExisting - should the variable be created if not existing. If set to "false" and if the variable is not existing, the set-operation will fail and return false.
		//		resetTime - defines the time, after which the setValue is changed backed to the original value. Specify 0.0f (or less) to set it permanently. Can be used as cooldowns to prevent spaming of responses.
		// Returns:
		//      Returns if the Set operation was successful. 
		// See Also:
		//      CreateVariable, SetVariableValue (by Variable-Pointer), GetVariable
		virtual bool SetVariableValue(const CHashedString& name, int newValue, bool createIfNotExisting = true, float resetTime = -1.0f) = 0;
		virtual bool SetVariableValue(const CHashedString& name, float newValue , bool createIfNotExisting = true, float resetTime = -1.0f) = 0;
		virtual bool SetVariableValue(const CHashedString& name, const char* newValue, bool createIfNotExisting = true, float resetTime = -1.0f) = 0;
		
		// <title SetVariableValue (by Variable-Pointer)>
		// Syntax: SetVariableValue(m_pDrsHealth, 0.5f, 0.0f)
		// Description:
		//      Will set the value of the specified variable to the new value. The variable pointer must be created by the same variable collection. Otherwise the behavior is undefined.
		//		The Variable-pointer can be obtained via "GetVariable" or when created via "CreateVariable".
		//		The value-type of the variable and the value-type of the parameter needs to fit. Otherwise the behavior is undefined.
		// Arguments:
		//      pVariable - a pointer to a variable created by / fetched from this variable collection.
		//      newValue - the value to set. The type of the newValue must match the previous type of the variable
		//		resetTime - defines the time, after which the setValue is changed backed to the original value. Specify 0.0f (or less) to set it permanently. Can be used as cooldowns to prevent spaming of responses.
		// Returns:
		//      Returns if the Set operation was successful. 
		// See Also:
		//      CreateVariable, SetVariableValue (by Variablename), GetVariable
		virtual bool SetVariableValue(CResponseVariable* pVariable, int newValue, float resetTime = -1.0f) = 0;
		virtual bool SetVariableValue(CResponseVariable* pVariable, float newValue, float resetTime = -1.0f) = 0;
		virtual bool SetVariableValue(CResponseVariable* pVariable, const char* newValue, float resetTime = -1.0f) = 0;

		// <title GetVariable>
		// Syntax: ResponseVariable* pOftenUsedVariable = GetVariable("Health");
		// Description:
		//      Will Fetch the variable from the collection with the specified name.
		// Arguments:
		// Returns:
		//      Returns a pointer the variable, or NULL if no variable with the given name exists in this collection.
		// See Also:
		//      CreateVariable, SetVariableValue (by Variable Pointer), SetVariableValue (by Variablename)
		virtual CResponseVariable* GetVariable(const CHashedString& name) const = 0;

		// <title GetNameAsString>
		// Syntax: string collectionNameAsString = m_pMyCollection->GetName().GetText();
		// Description:
		//      Will return the name of the collection. 
		// Arguments:
		// Returns:
		//      Returns the identifier that the DRS gave to this Variable collection.
		// See Also:
		//      CreateVariable, SetVariableValue (by Variable Pointer), SetVariableValue (by Variablename)
		virtual const CHashedString& GetName() const = 0;

		// <title Serialize>
		// Syntax: Called by the serialization library to serialize to disk or to the UI
		// Description:
		//      Serializes all the member variables of this class
		// Arguments:
		// Returns:
		// See Also:
		virtual void Serialize(Serialization::IArchive& ar) = 0;
	};
	
	struct IResponseActor
	{
		virtual ~IResponseActor() {}

		// <title GetName>
		// Syntax: string actorNameAsString = pActor->GetName().GetText();
		// Description:
		//      Will return the name of the Actor in the DRS. This is the name, with which the actor can be found.
		// Arguments:
		// Returns:
		//      Returns the identifier that the DRS gave to the Actor.
		// See Also:
		//      IDynamicResponseSystem::CreateResponseActor
		virtual const CHashedString& GetName() const = 0;

		// <title GetLocalVariables>
		// Syntax: pActor->GetLocalVariables()
		// Description:
		//      Will return the (local) variable collection for this actor.
		//		The variables in there can be modified.
		// Arguments:
		// Returns:
		//      Returns the local variable collection for the actor
		// See Also:
		//      IDynamicResponseSystem::CreateResponseActor
		virtual IVariableCollection* GetLocalVariables() = 0;

		// <title GetLocalVariables const>
		// Syntax: pActor->GetLocalVariables()
		// Description:
		//      Will return the (local) variable collection for this actor.
		//		The variables in there can NOT be modified in this const version.
		// Arguments:
		// Returns:
		//      Returns the local variable collection for the actor
		// See Also:
		//      IDynamicResponseSystem::CreateResponseActor
		virtual const IVariableCollection* GetLocalVariables() const = 0;

		// <title GetUserData>
		// Syntax: pActor->GetUserData()
		// Description:
		//      Will return the data, that was attached to the actor on creation time
		//		Can be used to store a connection to game specific representation of the actors.
		//		This is important, because the game can implement response-action, and will get only
		//		a IResponseActor pointer as a parameter. So it needs to be able to convert from this 
		//		class to its own gameobject class.
		// Arguments:
		// Returns:
		//      Returns the user data attached to the IResponseActor
		// See Also:
		//      IDynamicResponseSystem::CreateResponseActor
		virtual uint GetUserData() const = 0;
	};

	typedef std::shared_ptr<IVariableCollection> IVariableCollectionSharedPtr;

	struct IResponseInstance
	{
		virtual ~IResponseInstance() {}

		// <title GetCurrentResponder>
		// Syntax: pInstance->GetCurrentResponder()
		// Description:
		//      Will return the ResponseActor that is currently active in the ResponseInstance
		// Arguments:
		// Returns:
		//      Return the ResponseActor that is currently active in the ResponseInstance
		// See Also:
		//      IResponseAction::Execute, IResponseActor
		virtual IResponseActor* GetCurrentResponder() const = 0;

		// <title SetCurrentResponder>
		// Syntax: pInstance->SetCurrentResponder(pActorToChangeTo)
		// Description:
		//      With this method the currently active Actor of the response instance can be changed.
		//		all following actions and conditions will therefore use the new actor.
		// Arguments:
		//	pNewResponder - the actor that should be the new active object for the response
		// Returns:
		// See Also:
		//      IDynamicResponseSystem::GetResponseActor
		virtual void SetCurrentResponder(IResponseActor* pNewResponder) = 0;

		// <title GetOriginalSender>
		// Syntax: pInstance->GetOriginalSender()
		// Description:
		//      Will return the EesponseActor that fired the signal in the first place
		// Arguments:
		// Returns:
		//      return the EesponseActor that fired the signal in the first place
		// See Also:
		//      IResponseAction::Execute, IDynamicResponseSystem::QueueSignal 
		virtual IResponseActor* GetOriginalSender() const = 0;

		// <title GetSignalName>
		// Syntax: pInstance->GetSignalName()
		// Description:
		//      Will return the name of the signal that we currently respond to.
		// Arguments:
		// Returns:
		//      returns the name of the signal that we currently respond to.
		// See Also:
		//      IResponseAction::Execute, IDynamicResponseSystem::QueueSignal 
		virtual const CHashedString& GetSignalName() const = 0;

		// <title GetContextVariables>
		// Syntax: pInstance->GetContextVariables()
		// Description:
		//      Will return the context variable collection for this signal (if there was one specified when the signal was queued)
		// Arguments:
		// Returns:
		//      return the context variable collection for this signal (if there is one)
		// See Also:
		//      IResponseAction::Execute, IDynamicResponseSystem::QueueSignal 
		virtual IVariableCollectionSharedPtr GetContextVariables() const = 0;
	};
	
	struct IDynamicResponseSystem
	{
	public:
		virtual ~IDynamicResponseSystem() {}

		// <title Init>
		// Syntax: pDRS->Init(pPath)
		// Description:
		//      Will load all response definitions from the specified folder. Will also create all needed subsystems
		// Arguments:
		//		pFilesFolder - the folder where the response definition files are located
		// Returns:
		//      return if responses were loaded
		// See Also:
		virtual bool Init(const char* pFilesFolder) = 0;

		// <title Init>
		// Syntax: pDRS->ReInit(pPath)
		// Description:
		//      Will re-load all response definitions from the specified folder. Might be used for reloading-on-the-fly in the editor
		// Returns:
		//      return if responses were loaded
		// See Also:
		virtual bool ReInit() = 0;

		// <title Init>
		// Syntax: pDRS->Init(pPath)
		// Description:
		//      Will update all systems that are part of DRS. (signal handling, updating of running response instances, variable reset timers...)
		//		Needs to be called continuously.
		// Arguments:
		// Returns:
		// See Also:
		virtual void Update() = 0;

		// <title CreateVariableCollection>
		// Syntax: pDRS->CreateVariableCollection("myCollection")
		// Description:
		//      Will create a new variable collection with the specified name. Will fail if a collection with that name already exists.
		//		the created Collection needs to be released with ReleaseVariableCollection when not needed anymore.
		// Arguments:
		//		name - the name of the new collection. needs to be unique
		// Returns:
		//      return the newly created Collection when successful. 
		// See Also:
		//		ReleaseVariableCollection
		virtual IVariableCollection* CreateVariableCollection(const CHashedString& name) = 0;

		// <title ReleaseVariableCollection>
		// Syntax: pDRS->ReleaseVariableCollection(pMyCollection)
		// Description:
		//      Will release the given VariableCollection. The collection must have be generated with CreateVariableCollection.
		// Arguments:
		//		pToBeReleased - Collection to be released
		// Returns:
		// See Also:
		//		CreateVariableCollection
		virtual void ReleaseVariableCollection(IVariableCollection* pToBeReleased) = 0;

		// <title GetCollection>
		// Syntax: pDRS->GetCollection("myCollection")
		// Description:
		//      Will return the Variable collection with the specified name, if existing. 
		// Arguments:
		//		name - the name of the new collection. 
		// Returns:
		//      returns the Variable collection with the specified name, if existing. Otherwise 0
		// See Also:
		//		CreateVariableCollection
		virtual IVariableCollection* GetCollection(const CHashedString& name) = 0;

		// <title GetCollection>
		// Syntax: pDRS->CreateContextCollection()
		// Description:
		//      Will return a new Variable collection that can be used as a context variable collection for sent signals.
		//			The DRS will not hold a reference to this created collection. So it will be released, when no one is referencing it anymore on the outside.
		// Arguments:
		// Returns:
		//      returns the empty variable collection.
		// See Also:
		//		QueueSignal, CreateVariableCollection
		virtual IVariableCollectionSharedPtr CreateContextCollection() = 0;

		// <title QueueSignal>
		// Syntax: pDRS->QueueSignal("si_EnemySpotted", pMyActor, pContextVariableCollection")
		// Description:
		//      Will queue a new signal. it will be handled in the next update of the DRS. The drs will determine if there is a response for the signal.
		// Arguments:
		//		signalName - the name of the signal
		//		pSender - the object on which the response is started to execute
		//		pSignalContext - if for the processing of the signal some additional input is required, these is the place to store it. The Signal will hold a reference as long as needed.
		//		delayBeforeFiring - time that the signal is queued before processed
		// Returns:
		// See Also:
		//		CreateContextCollection
		virtual void QueueSignal(const CHashedString& signalName, IResponseActor* pSender, DRS::IVariableCollectionSharedPtr pSignalContext = nullptr, float delayBeforeFiring = 0.0f) = 0;

		// <title CancelSignalProcessing>
		// Syntax: pDRS->CancelSignalProcessing("si_EnemySpotted", pMyActor)
		// Description:
		//      Will stop the execution of any response that was started by exactly this signal on the given object.
		//		REMARK: Its up to the actions, how to handle cancel requests, they might not stop immediately.
		//		REMARK: You will have to specify the object that has send the signal originally, remember that the active actor can change during the execution of the response
		// Arguments:
		//		signalName - Name of the signal for which we want to interrupt the execution of responses
		//		pSender - the Actor for which we want to stop the execution
		//		delayBeforeCancel - delay before we stop the execution
		// Returns:
		// See Also:
		virtual void CancelSignalProcessing(const CHashedString& signalName, IResponseActor* pSender, float delayBeforeCancel = 0.0f) = 0;

		// <title CreateResponseActor>
		// Syntax: pDRS->CreateResponseActor("Bob", myGameSpecificEntityGUID)
		// Description:
		//      Will create a new Response Actor. This actor is registered in the DRS.
		//		All actors that will be used in the DRS needs to be created first, otherwise you wont be able to queue Signals for them or set them as a actor by name
		// Arguments:
		//		pActorName - the unique name of the new actor. if a actor with that name already exist the behavior is undefined
		//		userData - this data will be attached to the actor. it can be obtained from the actor with the getUserData method. This is the link between your game-entity and the DRS-actor.
		// Returns:
		//      return the newly created Actor when successful. 
		// See Also:
		//		ReleaseResponseActor, IResponseActor::GetUserData
		virtual IResponseActor* CreateResponseActor(const CHashedString& actorName, const uint userData = 0) = 0;

		// <title ReleaseResponseActor>
		// Syntax: pDRS->ReleaseResponseActor(pMyActor)
		// Description:
		//      Will release the specified Actor. The actor cannot be used/found by the DRS afterwards.
		// Arguments:
		//		pActorToFree - the actor that should be released
		// Returns:
		// See Also:
		//		CreateResponseActor
		virtual bool ReleaseResponseActor(IResponseActor* pActorToFree) = 0;

		// <title GetResponseActor>
		// Syntax: pDRS->GetResponseActor("human_01")
		// Description:
		//      Will return the actor with the specified name if existing. 0 otherwise
		// Arguments:
		//		pActorName - the name of the actor to look for
		// Returns:
		//      return the found actor, or 0 if not found
		// See Also:
		//		CreateResponseActor
		virtual IResponseActor* GetResponseActor(const CHashedString& actorName) = 0;

		// <title Reset>
		// Syntax: pDRS->Reset()
		// Description:
		//      Will reset everything in the dynamic response system. So it will reset all variables and stop all signal responses. it wont remove registered actions or conditions or created actors.
		// Arguments:
		//		resetHint - a hint what to reset. For example you could decide to only reset variables instead of destroy them.
		// Returns:
		// See Also:
		virtual void Reset(uint32 resetHint = -1) = 0;

		// <title Reset>
		// Syntax: pDRS->GetCustomDataformatHelper()
		// Description:
		//      Will return a helper class, which allows you to create DRS structures from your own data format. (e.g. used by the excel table importer)
		// Arguments:
		//		resetHint - a hint what to reset. For example you could decide to only reset variables instead of destroy them.
		// Returns:
		// See Also:
		virtual IDataImportHelper* GetCustomDataformatHelper() = 0;

		enum eSerializationFilter
		{
			eSerializationFilter_Responses = BIT(0),
			eSerializationFilter_Variables = BIT(1),
			eSerializationFilter_DebugData_1 = BIT(2),
			eSerializationFilter_DebugData_2 = BIT(3),
			eSerializationFilter_DebugData_3 = BIT(4),
			eSerializationFilter_DebugData_4 = BIT(5),
			///...
			eSerializationFilter_Special = BIT(30),
			
		};

		// <title Serialize>
		// Syntax: Called by the serialization library to serialize to disk or to the UI
		// Description:
		//      Serializes all the member variables of this class
		// Arguments:
		// Returns:
		// See Also:
		virtual void Serialize(Serialization::IArchive& ar) = 0;
		
		// <title GetActionSerializationFactory>
		// Syntax: Used by the serialization library to get a factory class that constructs actions
		// Description:
		//      Gives access to a factory to construct actions
		// Arguments:
		// Returns:
		//			A factory to construct actions
		// See Also:
		virtual ActionSerializationClassFactory& GetActionSerializationFactory() = 0;

		// <title GetConditionSerializationFactory>
		// Syntax: Used by the serialization library to get a factory class that constructs conditions
		// Description:
		//      Gives access to a factory to construct conditions
		// Arguments:
		// Returns:
		//			A factory to construct conditions
		// See Also:
		virtual ConditionSerializationClassFactory& GetConditionSerializationFactory() = 0;

	};

}  //namespace DRS

#endif  //IDYNAMICREPONSESYSTEM_H_INCLUDED
