#pragma once

#include <QObject>
#include <QPointF>
#include <maya/MCallbackIdArray.h>
#include "mayaMVG/qt/MVGMayaViewportEvent.h"

namespace mayaMVG {

class MVGWindowEventFilter: public QObject {

	public:
		MVGWindowEventFilter(const MCallbackIdArray& ids, MVGMayaViewportMouseEventFilter* m, QObject* parent);

	protected:
		bool eventFilter(QObject *obj, QEvent *e);

	private:
		MCallbackIdArray m_ids;
		MVGMayaViewportMouseEventFilter* m_mouseFilter;
};



class MVGContextEventFilter: public QObject {
	
	public:
		MVGContextEventFilter(MVGContext* ctx, QObject* parent);

	protected:
		bool eventFilter(QObject *obj, QEvent *e);

	private:
		MVGContext* m_context;
};

} // mayaMVG
