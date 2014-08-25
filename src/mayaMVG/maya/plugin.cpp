#include <maya/MFnPlugin.h>
#include "mayaMVG/core/MVGLog.h"
#include "mayaMVG/maya/MVGMayaUtil.h"
#include "mayaMVG/maya/cmd/MVGCmd.h"
#include "mayaMVG/maya/cmd/MVGEditCmd.h"
#include "mayaMVG/qt/MVGMainWidget.h"
#include "mayaMVG/maya/context/MVGContextCmd.h"
#include "mayaMVG/maya/context/MVGCreateManipulator.h"
#include "mayaMVG/maya/context/MVGMoveManipulator.h"
#include <maya/MSelectionList.h>
#include <maya/MEventMessage.h>
#include <maya/MMessage.h>


using namespace mayaMVG;

MCallbackIdArray _mayaCallbackIds;

static void selectionChangedCB(void* /*userData*/) {
	QWidget* menuLayout = MVGMayaUtil::getMVGMenuLayout();
	if(!menuLayout)
		return;
	MVGMainWidget* mvgMainWidget = menuLayout->findChild<MVGMainWidget*>("mvgMainWidget");
	if(!mvgMainWidget)
		return;
	MDagPath path;
	MObject component;
	MSelectionList list;
	MGlobal::getActiveSelectionList(list);
	QList<QString> selectedCameras;
	for ( size_t i = 0; i < list.length(); i++) {
		list.getDagPath(i, path, component);
		path.extendToShape();
		if(path.isValid() 
			&& ((path.child(0).apiType() == MFn::kCamera) || (path.apiType() == MFn::kCamera))) {
			MFnDependencyNode fn(path.transform());
			selectedCameras.push_back(fn.name().asChar());
		}
	}
	mvgMainWidget->getProjectWrapper().selectItems(selectedCameras);
}

void currentContextChangedCB(void* /*userData*/)
{
	QWidget* menuLayout = MVGMayaUtil::getMVGMenuLayout();
	if(!menuLayout)
		return;
	MVGMainWidget* mvgMainWidget = menuLayout->findChild<MVGMainWidget*>("mvgMainWidget");
	if(!mvgMainWidget)
		return;
	MString context;
	MStatus status;
	status = MVGMayaUtil::getCurrentContext(context);
	CHECK(status)
	mvgMainWidget->getProjectWrapper().setCurrentContext(QString(context.asChar()));
}

void sceneChangedCB(void* /*userData*/)
{
	QWidget* menuLayout = MVGMayaUtil::getMVGMenuLayout();
	if(!menuLayout)
		return;
	MVGMainWidget* mvgMainWidget = menuLayout->findChild<MVGMainWidget*>("mvgMainWidget");
	if(!mvgMainWidget)
		return;
	MGlobal::executePythonCommand("from mayaMVG import window;\n"
								"window.mvgReloadPanels()");
	mvgMainWidget->getProjectWrapper().loadExistingProject();
	MGlobal::executeCommand("mayaMVGTool -e -rebuild mayaMVGTool1");
}

static void undoCB(void * /*userData*/)
{
	// TODO : rebuild only the mesh modified
	// TODO : don't rebuild on action that don't modify any mesh
	MString redoName;
	MVGMayaUtil::getRedoName(redoName);        
	// Don't rebuild if selection action
	int spaceIndex = redoName.index(' ');
	MString cmdName = redoName.substring(0, spaceIndex - 1);
	if(cmdName != "select" && cmdName != "miCreateDefaultPresets")
		MGlobal::executeCommand("mayaMVGTool -e -rebuild mayaMVGTool1");
}

static void redoCB(void * /*userData*/)
{
	MString undoName;
	MVGMayaUtil::getUndoName(undoName);
	// Don't rebuild if selection action
	int spaceIndex = undoName.index(' ');
	MString cmdName = undoName.substring(0, spaceIndex - 1);
	if(cmdName != "select" && cmdName != "miCreateDefaultPresets")
		MGlobal::executeCommand("mayaMVGTool -e -rebuild mayaMVGTool1");
}


MStatus initializePlugin(MObject obj) {
	MStatus status;
	MFnPlugin plugin(obj, PLUGIN_COMPANY, "1.0", "Any");

	// register commands
	status = plugin.registerCommand("MVGCmd", MVGCmd::creator);
	// register context
	status = plugin.registerContextCommand(MVGContextCmd::name, &MVGContextCmd::creator, 
								MVGEditCmd::name, MVGEditCmd::creator, MVGEditCmd::newSyntax);
	// register nodes
	status = plugin.registerNode("MVGCreateManipulator", MVGCreateManipulator::_id, 
								&MVGCreateManipulator::creator, &MVGCreateManipulator::initialize, 
								MPxNode::kManipulatorNode);
	status = plugin.registerNode("MVGMoveManipulator", MVGMoveManipulator::_id, 
								&MVGMoveManipulator::creator, &MVGMoveManipulator::initialize, 
								MPxNode::kManipulatorNode);
	// register ui
	status = plugin.registerUI("mayaMVGCreateUI", "mayaMVGDeleteUI");

	MVGMayaUtil::createMVGContext();

	// Maya callbacks
	_mayaCallbackIds.append(MEventMessage::addEventCallback("PostToolChanged", currentContextChangedCB));
	_mayaCallbackIds.append(MEventMessage::addEventCallback("NewSceneOpened", sceneChangedCB));
	_mayaCallbackIds.append(MEventMessage::addEventCallback("SceneOpened", sceneChangedCB));
	_mayaCallbackIds.append(MEventMessage::addEventCallback("Undo", undoCB));
	_mayaCallbackIds.append(MEventMessage::addEventCallback("Redo", redoCB));
	_mayaCallbackIds.append(MEventMessage::addEventCallback("SelectionChanged", selectionChangedCB));

	if (!status)
		LOG_ERROR("unexpected error");
	return status;
}


MStatus uninitializePlugin(MObject obj) {
	MStatus status;
	MFnPlugin plugin(obj);

	MVGMayaUtil::deleteMVGContext();
	MVGMayaUtil::deleteMVGWindow();

     // Remove callbacks
    status = MMessage::removeCallbacks(_mayaCallbackIds);

	// deregister commands
	status = plugin.deregisterCommand("MVGCmd");
	// deregister context
	status = plugin.deregisterContextCommand(MVGContextCmd::name, MVGEditCmd::name);
	// deregister nodes
	status = plugin.deregisterNode(MVGCreateManipulator::_id);
	status = plugin.deregisterNode(MVGMoveManipulator::_id);
    
	MVGMayaUtil::deleteMVGContext();

	if (!status)
		LOG_ERROR("unexpected error");
	return status;
}
