#include "MZSceneTree.h"

MZSceneTree::MZSceneTree()
{
	Root = TSharedPtr<FolderNode>(new FolderNode);
	Root->Name = FString("UE5");
	Root->Parent = nullptr;
}

TSharedPtr<FolderNode> MZSceneTree::FindOrAddChildFolder(TSharedPtr<TreeNode> node, FString name, TSharedPtr<TreeNode>& mostRecentParent)
{
	for (auto child : node->Children)
	{
		if (child->Name == name && child->GetAsFolderNode())
		{
			return StaticCastSharedPtr<FolderNode>(child);
		}
	}
	TSharedPtr<FolderNode> newChild(new FolderNode);
	newChild->Parent = node;
	newChild->Name = name;
	newChild->Id = FGuid::NewGuid();
	node->Children.push_back(newChild);
	NodeMap.Add(newChild->Id, newChild);
	if (!mostRecentParent)
	{
		mostRecentParent = newChild;
	}
	return newChild;
}



void MZSceneTree::Clear()
{
	ClearRecursive(Root);
	NodeMap.Empty();
	NodeMap.Add(Root->Id, Root);
}

void MZSceneTree::ClearRecursive(TSharedPtr<TreeNode> node)
{
	for (auto child : node->Children)
	{
		ClearRecursive(child);
	}
	if (node == Root)
	{
		Root->Children.clear();
		return;
	}
}

TSharedPtr<ActorNode> MZSceneTree::AddActor(FString folderPath, AActor* actor)
{
	TSharedPtr<TreeNode> mostRecentParent;
	return AddActor(folderPath, actor, mostRecentParent);
}

TSharedPtr<ActorNode> MZSceneTree::AddActor(FString folderPath, AActor* actor, TSharedPtr<TreeNode>& mostRecentParent)
{
	if (!actor)
	{
		return nullptr;
	}

	folderPath.RemoveFromStart(FString("None"));
	folderPath.RemoveFromStart(FString("/"));
	TArray<FString> folders;
	folderPath.ParseIntoArray(folders, TEXT("/"));
	
	TSharedPtr<TreeNode> ptr = Root;
	for (auto item : folders)
	{
		ptr = FindOrAddChildFolder(ptr, item, mostRecentParent);
	}

	TSharedPtr<ActorNode> newChild(new ActorNode);
	newChild->Parent = ptr;
	newChild->Name = actor->GetActorLabel();
	newChild->Id = actor->GetActorGuid();
	newChild->actor = MZActorReference(actor);
	newChild->NeedsReload = true;
	ptr->Children.push_back(newChild);
	NodeMap.Add(newChild->Id, newChild);

	if (actor->GetRootComponent())
	{
		TSharedPtr<SceneComponentNode> loadingChild(new SceneComponentNode);
		loadingChild->Name = "Loading...";
		loadingChild->Id = FGuid::NewGuid();
		loadingChild->Parent = newChild;
		newChild->Children.push_back(loadingChild);
	}
	if (!mostRecentParent)
	{
		mostRecentParent = newChild;
	}
	return newChild;
}

TSharedPtr<ActorNode> MZSceneTree::AddActor(TSharedPtr<TreeNode> parent, AActor* actor)
{
	if (!actor)
	{
		return nullptr;
	}
	if (!parent)
	{
		parent = Root;
	}

	TSharedPtr<ActorNode> newChild(new ActorNode);
	newChild->Parent = parent;
	newChild->Name = actor->GetActorLabel();
	newChild->Id = actor->GetActorGuid();
	newChild->actor = MZActorReference(actor);
	newChild->NeedsReload = true;
	parent->Children.push_back(newChild);
	NodeMap.Add(newChild->Id, newChild);

	if (actor->GetRootComponent())
	{
		TSharedPtr<SceneComponentNode> loadingChild(new SceneComponentNode);
		loadingChild->Name = "Loading...";
		loadingChild->Id = FGuid::NewGuid();
		loadingChild->Parent = newChild;
		newChild->Children.push_back(loadingChild);
	}

	return newChild;
}

TSharedPtr<SceneComponentNode> MZSceneTree::AddSceneComponent(TSharedPtr<ActorNode> parent, USceneComponent* sceneComponent)
{
	TSharedPtr<SceneComponentNode>newComponentNode(new SceneComponentNode);
	newComponentNode->sceneComponent = MZComponentReference(sceneComponent);
	newComponentNode->Id = FGuid::NewGuid();
	newComponentNode->Name = sceneComponent->GetFName().ToString();
	newComponentNode->Parent = parent;
	newComponentNode->NeedsReload = true;
	parent->Children.push_back(newComponentNode);
	NodeMap.Add(newComponentNode->Id, newComponentNode);

	TSharedPtr<SceneComponentNode> loadingChild(new SceneComponentNode);
	loadingChild->Name = "Loading...";
	loadingChild->Id = FGuid::NewGuid();
	loadingChild->Parent = newComponentNode;
	newComponentNode->Children.push_back(loadingChild);

	return newComponentNode;
}

TSharedPtr<SceneComponentNode> MZSceneTree::AddSceneComponent(TSharedPtr<SceneComponentNode> parent, USceneComponent* sceneComponent)
{
	TSharedPtr<SceneComponentNode> newComponentNode(new SceneComponentNode);
	newComponentNode->sceneComponent = MZComponentReference(sceneComponent);
	newComponentNode->Id = FGuid::NewGuid();
	newComponentNode->Name = sceneComponent->GetFName().ToString();
	newComponentNode->Parent = parent;
	newComponentNode->NeedsReload = true;
	parent->Children.push_back(newComponentNode);
	NodeMap.Add(newComponentNode->Id, newComponentNode);

	TSharedPtr<SceneComponentNode> loadingChild(new SceneComponentNode);
	loadingChild->Name = "Loading...";
	loadingChild->Id = FGuid::NewGuid();
	loadingChild->Parent = newComponentNode;
	newComponentNode->Children.push_back(loadingChild);

	return newComponentNode;
}


flatbuffers::Offset<mz::fb::Node> TreeNode::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes = SerializeChildren(fbb);
	return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&Id, TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*GetClassDisplayName()), false, true, 0, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb, &childNodes).Union(), "UE5", 0, 0);
}

flatbuffers::Offset<mz::fb::Node> ActorNode::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> metadata;
	for (auto [key, value] : mzMetaData)
	{
		metadata.push_back(mz::fb::CreateMetaDataEntryDirect(fbb, TCHAR_TO_UTF8(*key), TCHAR_TO_UTF8(*value)));
	}

	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes = SerializeChildren(fbb);
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins = SerializePins(fbb);
	return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&Id, TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*GetClassDisplayName()), false, true, &pins, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb, &childNodes).Union(), "UE5", 0, 0, 0, 0, &metadata);
}

std::vector<flatbuffers::Offset<mz::fb::Pin>> ActorNode::SerializePins(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins;
	for (auto mzprop : Properties)
	{
		pins.push_back(mzprop->Serialize(fbb));
	}
	return pins;
}

ActorNode::~ActorNode()
{
}

flatbuffers::Offset<mz::fb::Node> SceneComponentNode::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes = SerializeChildren(fbb);
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins = SerializePins(fbb);
	return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&Id, TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*GetClassDisplayName()), false, true, &pins, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb, &childNodes).Union(), "UE5", 0, 0);
}

std::vector<flatbuffers::Offset<mz::fb::Pin>> SceneComponentNode::SerializePins(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins;
	for (auto mzprop : Properties)
	{
		pins.push_back(mzprop->Serialize(fbb));
	}
	return pins;
}

std::vector<flatbuffers::Offset<mz::fb::Node>> TreeNode::SerializeChildren(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes;

	if (Children.empty())
	{
		return childNodes;
	}

	for (auto child : Children)
	{
		childNodes.push_back(child->Serialize(fbb));
	}

	return childNodes;
}

TreeNode::~TreeNode()
{
}


SceneComponentNode::~SceneComponentNode()
{
}

FolderNode::~FolderNode()
{
}


