//
// Created by vr1s on 5/7/23.
//

#include "DockableSidebar.h"
#include "binaryninjacore.h"
#include "ui/theme.h"
#include "ui/sidebar.h"
#include "priv.h"
#include "xreflist.h"
#include "typeview.h"
#include "variablelist.h"
#include "stackview.h"
#include "stringsview.h"
#include "taglist.h"
#include "minigraph.h"
#include "memorymap.h"

#include <QPainter>
#include <QStyleOptionButton>
#include <QDebug>
#include <QStylePainter>

double dist(QPointF p, QPointF q)
{
    auto dx = p.x() - q.x();
    auto dy = p.y() - q.y();
    auto dist = sqrt(abs(dx * dx) + abs(dy * dy));
    return dist;
}

DockableSidebar *ContextSidebarManager::SidebarForGlobalPos(QPointF pos)
{
    auto nearest = m_activeSidebars.at(0);
    double nearestDist = dist(pos, nearest->mapToGlobal(nearest->pos()));
    for (auto sidebar: m_activeSidebars)
    {
        auto d = dist(pos, sidebar->mapToGlobal(sidebar->pos()));
        if (d < nearestDist)
        {
            nearest = sidebar;
            nearestDist = d;
        }
    }
    return nearest;
}

void ContextSidebarManager::SetupSidebars()
{
    auto wholeMainViewSplitter = new DragAcceptingSplitter(this, nullptr);
    auto central = m_context->mainWindow()->centralWidget();

    auto leftSideFrame = new QFrame(central);
    QPalette pal = leftSideFrame->palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    leftSideFrame->setContentsMargins(0, 0, 0, 0);

    m_leftContentView = new DockableSidebarContentView(this, m_context, wholeMainViewSplitter, true, central);
    m_leftContentView->setObjectName("leftContentView");

    m_sidebarForPos[TopLeft] = new DockableSidebar(this, TopLeft, m_leftContentView, leftSideFrame);
    auto leftSidebarLayout = new QVBoxLayout(leftSideFrame);

    leftSidebarLayout->setContentsMargins(0, 0, 0, 0);
    leftSidebarLayout->addWidget(m_sidebarForPos[TopLeft]);
    leftSidebarLayout->addStretch();
    m_sidebarForPos[BottomLeft] = new DockableSidebar(this, BottomLeft, m_leftContentView, leftSideFrame);
    leftSidebarLayout->addWidget(m_sidebarForPos[BottomLeft]);

    /* Right */

    m_rightContentView = new DockableSidebarContentView(this, m_context, wholeMainViewSplitter, false, central);

    auto rightSideFrame = new QFrame(central);
    rightSideFrame->setContentsMargins(0, 0, 0, 0);

    m_sidebarForPos[TopRight] = new DockableSidebar(this, TopRight, m_rightContentView, rightSideFrame);
    auto rightSidebarLayout = new QVBoxLayout(rightSideFrame);

    rightSidebarLayout->setContentsMargins(0, 0, 0, 0);
    rightSidebarLayout->addWidget(m_sidebarForPos[TopRight]);
    rightSidebarLayout->addStretch();
    m_sidebarForPos[BottomRight] = new DockableSidebar(this, BottomRight, m_rightContentView, rightSideFrame);
    rightSidebarLayout->addWidget(m_sidebarForPos[BottomRight]);

    wholeMainViewSplitter->setContentsMargins(0, 0, 0, 0);
    central->setContentsMargins(0, 0, 0, 0);
    wholeMainViewSplitter->addWidget(m_leftContentView);

    // sorry guys :3
    for (auto child: central->children())
    {
        if (std::string(child->metaObject()->className()) == "QSplitter")
        {
            auto mainView = qobject_cast<QSplitter *>(child);
            wholeMainViewSplitter->addWidget(mainView);
            break;
        }
    }
    auto frm = new QFrame(central);
    m_wholeLayout = new QHBoxLayout(frm);
    m_wholeLayout->setContentsMargins(0, 0, 0, 0);
    central->layout()->addWidget(frm);
    m_wholeLayout->addWidget(leftSideFrame);
    wholeMainViewSplitter->setParent(central);
    wholeMainViewSplitter->addWidget(m_rightContentView);
    m_wholeLayout->addWidget(wholeMainViewSplitter);
    m_wholeLayout->addWidget(rightSideFrame);

    m_oldSidebar->setVisible(false);

    m_sidebarForPos[TopLeft]->m_containedTypes.push_back(new ComponentTreeSidebarWidgetType());
    m_sidebarForPos[TopLeft]->m_containedTypes.push_back(new TypeViewSidebarWidgetType());
    m_sidebarForPos[TopLeft]->m_containedTypes.push_back(new StringsViewSidebarWidgetType());

    m_sidebarForPos[TopRight]->m_containedTypes.push_back(new VariableListSidebarWidgetType());
    m_sidebarForPos[TopRight]->m_containedTypes.push_back(new StackViewSidebarWidgetType());
    m_sidebarForPos[TopRight]->m_containedTypes.push_back(new MemoryMapSidebarWidgetType());

    m_sidebarForPos[BottomLeft]->m_containedTypes.push_back(new CrossReferenceSidebarWidgetType());
    m_sidebarForPos[BottomLeft]->m_containedTypes.push_back(new MiniGraphSidebarWidgetType());

    m_sidebarForPos[BottomRight]->m_containedTypes.push_back(new TagListSidebarWidgetType());

    UpdateTypes();
}

void ContextSidebarManager::UpdateTypes()
{
    m_activeSidebars.clear();
    for (size_t i = 0; i < SidebarPosCount; i++)
    {
        if (auto it = m_sidebarForPos.find(SidebarPos(i)); it != m_sidebarForPos.end())
        {
            auto sidebar = it->second;
            auto replacement = new DockableSidebar(this, sidebar->m_sidebarPos, sidebar->m_contentView,
                                                   sidebar->parentWidget());
            replacement->ClearTypes();
            for (auto type: sidebar->m_containedTypes)
                replacement->AddType(type);
            replacement->UpdateForTypes();
            m_sidebarForPos[sidebar->m_sidebarPos] = replacement;
            sidebar->parentWidget()->layout()->replaceWidget(sidebar, replacement);
            sidebar->deleteLater();
            m_activeSidebars.push_back(replacement);
            replacement->HighlightActiveButton();
        }
    }
}

DragAcceptingSplitter::DragAcceptingSplitter(ContextSidebarManager* context, QWidget* parent)
    : QSplitter(parent), m_context(context)
{
    setAcceptDrops(true);
}

void DragAcceptingSplitter::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/plain"))
        event->acceptProposedAction();
}

void DragAcceptingSplitter::dropEvent(QDropEvent *event)
{
    auto type = m_context->m_currentDragTarget->m_type;
    m_context->m_leftContentView->DeactivateWidgetType(type);
    m_context->m_rightContentView->DeactivateWidgetType(type);
    m_context->UpdateTypes();

    SidebarWidgetAndHeader *existingWidget = nullptr;
    ViewFrame *currentViewFrame = m_context->m_context->getCurrentViewFrame();
    QString currentDataType = currentViewFrame ? currentViewFrame->getCurrentDataType() : QString();

    auto frameIter = m_context->m_widgets.find(type->viewSensitive() ? currentViewFrame : nullptr);
    if (frameIter != m_context->m_widgets.end())
    {
        auto dataIter = frameIter->second.find(type->viewSensitive() ? currentDataType : QString());
        if (dataIter != frameIter->second.end())
        {
            auto widgetIter = dataIter->second.find(type);
            if (widgetIter != dataIter->second.end())
                existingWidget = widgetIter->second;
        }
    }
    if (existingWidget)
    {
        m_context->m_widgets[m_context->m_context->getCurrentViewFrame()][currentDataType].erase(type);
    }
    SidebarWidget *widget;
    SidebarWidgetAndHeader *contents;
    if (type->viewSensitive())
    {
        if (m_context->m_context->getCurrentViewFrame())
        {
            widget = type->createWidget(m_context->m_context->getCurrentViewFrame(), m_context->m_context->getCurrentView()->getData());
            if (!widget)
                widget = type->createInvalidContextWidget();
        } else
            widget = type->createInvalidContextWidget();
        contents = new SidebarWidgetAndHeader(widget, m_context->m_context->getCurrentViewFrame());

        // Send notifications for initial state
        if (m_context->m_context->getCurrentViewFrame())
            widget->notifyViewChanged(m_context->m_context->getCurrentViewFrame());
    } else
    {
        widget = type->createWidget(nullptr, nullptr);
        if (!widget)
            widget = type->createInvalidContextWidget();
        contents = new SidebarWidgetAndHeader(widget, m_context->m_context->getCurrentViewFrame());
    }

    if (contents)
    {
        //contents->setParent(nullptr);
        auto globPos = mapToGlobal(event->position()).toPoint();
        contents->setGeometry(globPos.x(), globPos.y(), 350, m_context->m_context->mainWindow()->height());
        contents->show();
    }

}

OrientablePushButton::OrientablePushButton(DockableSidebar *parent)
        : QPushButton(parent)
{
    setCheckable(true);
    setStyleSheet(QString("QPushButton {border: 0px;} "
                          "QPushButton::hover {"
                          "background-color: ") + getThemeColor(SidebarBackgroundColor).darker(140).name()
                  + ";}");
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    if (parent)
        setOrientation((parent->m_sidebarPos == TopLeft || parent->m_sidebarPos == BottomLeft)
                       ? OrientablePushButton::VerticalBottomToTop : OrientablePushButton::VerticalTopToBottom);
    else
        setOrientation(OrientablePushButton::VerticalBottomToTop); // placeholder w/ no content doesn't matter :D
    setPalette(pal);

    m_sidebar = parent;
}

OrientablePushButton::OrientablePushButton(const QString &text, DockableSidebar *parent)
        : QPushButton(text, parent)
{
    setCheckable(true);
    setStyleSheet(QString("QPushButton {border: 0px;} "
                          "QPushButton::hover {"
                          "background-color: ") + getThemeColor(SidebarBackgroundColor).darker(140).name()
                  + ";}");
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    setOrientation((parent->m_sidebarPos == TopLeft || parent->m_sidebarPos == BottomLeft)
                   ? OrientablePushButton::VerticalBottomToTop : OrientablePushButton::VerticalTopToBottom);
    setPalette(pal);

    m_sidebar = parent;
}

OrientablePushButton::OrientablePushButton(const QString &text, QWidget *parent)
        : QPushButton(text, parent)
{
    setCheckable(true);
    setStyleSheet(QString("QPushButton {border: 0px;} "
                          "QPushButton::hover {"
                          "background-color: ") + getThemeColor(SidebarBackgroundColor).darker(140).name()
                  + ";}");
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    setOrientation(OrientablePushButton::VerticalTopToBottom);
    setPalette(pal);
}

OrientablePushButton::OrientablePushButton(const QIcon &icon, const QString &text, DockableSidebar *parent)
        : QPushButton(icon, text, parent)
{
    setCheckable(true);
    setStyleSheet(
            QString("QPushButton {border: 0px; background-color: " + getThemeColor(SidebarBackgroundColor).name() +
                    ";} "
                    "QPushButton::hover {"
                    "background-color: ") + getThemeColor(SidebarBackgroundColor).darker(140).name()
            + ";}");
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    setOrientation((parent->m_sidebarPos == TopLeft || parent->m_sidebarPos == BottomLeft)
                   ? OrientablePushButton::VerticalBottomToTop : OrientablePushButton::VerticalTopToBottom);
    setPalette(pal);

    m_sidebar = parent;
}

QSize OrientablePushButton::sizeHint() const
{
    QSize sh = QPushButton::sizeHint();

    if (m_Orientation != OrientablePushButton::Horizontal)
        sh.transpose();

    return sh;
}

void OrientablePushButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QStylePainter painter(this);
    QStyleOptionButton option;
    initStyleOption(&option);

    if (m_Orientation == OrientablePushButton::VerticalTopToBottom)
    {
        painter.rotate(90);
        painter.translate(0, -1 * width());
        option.rect = option.rect.transposed();
    } else if (m_Orientation == OrientablePushButton::VerticalBottomToTop)
    {
        painter.rotate(-90);
        painter.translate(-1 * height(), 0);
        option.rect = option.rect.transposed();
    }

    painter.drawControl(QStyle::CE_PushButton, option);
}

OrientablePushButton::Orientation OrientablePushButton::orientation() const
{
    return m_Orientation;
}

void OrientablePushButton::setOrientation(const OrientablePushButton::Orientation &orientation)
{
    m_Orientation = orientation;
}

void OrientablePushButton::mousePressEvent(QMouseEvent *event)
{
    if (m_sidebar && event->button() == Qt::LeftButton)
    {
        m_maybeDragStarted = true;
        m_mousePressPos = event->globalPosition();
        m_mouseMovePos = event->globalPosition();
        m_nearest = m_context->SidebarForGlobalPos(m_mousePressPos);
    }

    QPushButton::mousePressEvent(event);
    m_sidebar->HighlightActiveButton();
}

void OrientablePushButton::mouseMoveEvent(QMouseEvent *event)
{
    if (m_sidebar && !m_trashed)
    {
        if (event->buttons() == Qt::LeftButton)
        {
            if (m_dragTripCount < 6)
            {
                m_dragTripCount++;
                QPushButton::mouseMoveEvent(event);
                return;
            }
            if (m_maybeDragStarted)
            {
                m_beingDragged = true;
                m_context->DragStartedWithTarget(this);
                m_maybeDragStarted = false;
                raise();
                QDrag *drag = new QDrag(m_sidebar);
                QMimeData *mimeData = new QMimeData;

                mimeData->setText(m_type->name());
                drag->setMimeData(mimeData);
                drag->setPixmap(grab());
                m_sidebar->ButtonMovingOut(this);
                setVisible(false);
                m_trashed = true;
                drag->exec();
                return;
            }
        }
        if (!m_trashed)
            QPushButton::mouseMoveEvent(event);
    }
}

void OrientablePushButton::mouseReleaseEvent(QMouseEvent *event)
{
    // m_context->UpdateTypes();
    m_dragTripCount = 0;
    QPushButton::mouseReleaseEvent(event);
}

DockableSidebarContentView::DockableSidebarContentView(ContextSidebarManager *sidebarContext, UIContext *context,
                                                       QSplitter *parentSplitter, bool left, QWidget *parent)
        : QWidget(parent), m_sidebarCtx(sidebarContext), m_parentSplitter(parentSplitter), m_context(context),
          m_left(left)
{
    setContentsMargins(0, 0, 0, 0);
    m_splitter = new QSplitter(Qt::Orientation::Vertical, this);
    m_splitter->setContentsMargins(0, 0, 0, 0);
    setContentsMargins(0, 0, 0, 0);
    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(m_splitter, 0, 0);
    SizeCheck();
}

void DockableSidebarContentView::ActivateWidgetType(SidebarWidgetType *type, bool top, bool reset)
{
    if (!reset && top && m_topActive && m_topContents)
    {
        if (m_topContents->widget()->title().toStdString() == type->name().toStdString())
        {
            m_topContents->setParent(nullptr);
            m_topContents = nullptr;
            m_topType = nullptr;
            m_topActive = false;
            SizeCheck();
            return;
        }
    }
    if (!reset && !top && m_botActive && m_bottomContents)
    {
        if (m_bottomContents->widget()->title().toStdString() == type->name().toStdString())
        {
            m_bottomContents->setParent(nullptr);
            m_bottomContents = nullptr;
            m_bottomType = nullptr;
            m_botActive = false;
            SizeCheck();
            return;
        }
    }
    if (top)
        m_topType = type;
    else
        m_bottomType = type;

    SidebarWidgetAndHeader *existingWidget = nullptr;
    ViewFrame *currentViewFrame = m_sidebarCtx->m_context->getCurrentViewFrame();
    QString currentDataType = currentViewFrame ? currentViewFrame->getCurrentDataType() : QString();

    auto frameIter = m_sidebarCtx->m_widgets.find(type->viewSensitive() ? currentViewFrame : nullptr);
    if (frameIter != m_sidebarCtx->m_widgets.end())
    {
        auto dataIter = frameIter->second.find(type->viewSensitive() ? currentDataType : QString());
        if (dataIter != frameIter->second.end())
        {
            auto widgetIter = dataIter->second.find(type);
            if (widgetIter != dataIter->second.end())
                existingWidget = widgetIter->second;
        }
    }
    if (existingWidget)
    {
        if (top)
            m_topContents = existingWidget;
        else
            m_bottomContents = existingWidget;
        SizeCheck();
        if (top)
            SetTopWidget(existingWidget);
        else
            SetBottomWidget(existingWidget);
    }
    else
    {
        SidebarWidget *widget;
        SidebarWidgetAndHeader *contents;
        if (type->viewSensitive())
        {
            if (m_context->getCurrentViewFrame())
            {
                widget = type->createWidget(m_context->getCurrentViewFrame(), m_context->getCurrentView()->getData());
                if (!widget)
                    widget = type->createInvalidContextWidget();
            } else
                widget = type->createInvalidContextWidget();
            contents = new SidebarWidgetAndHeader(widget, m_context->getCurrentViewFrame());
            m_sidebarCtx->m_widgets[m_sidebarCtx->m_context->getCurrentViewFrame()][currentDataType][type] = contents;

            // Send notifications for initial state
            if (m_context->getCurrentViewFrame())
                widget->notifyViewChanged(m_context->getCurrentViewFrame());
        } else
        {
            widget = type->createWidget(nullptr, nullptr);
            if (!widget)
                widget = type->createInvalidContextWidget();
            contents = new SidebarWidgetAndHeader(widget, m_context->getCurrentViewFrame());
        }

        if (contents)
        {
            if (top)
                SetTopWidget(contents);
            else
                SetBottomWidget(contents);
        }
    }

    repaint();
}

void DockableSidebarContentView::DeactivateWidgetType(SidebarWidgetType* type)
{
    if (m_topActive && m_topType == type)
    {
        m_topContents->setParent(nullptr);
        m_topContents = nullptr;
        m_topType = nullptr;
        m_topActive = false;
        SizeCheck();
    }
    if (m_botActive && m_bottomType == type)
    {
        m_bottomContents->setParent(nullptr);
        m_bottomContents = nullptr;
        m_bottomType = nullptr;
        m_botActive = false;
        SizeCheck();
    }
}


void DockableSidebarContentView::SetTopWidget(SidebarWidgetAndHeader *widget)
{
    m_topContents = widget;
    if (m_topActive)
    {
        if (widget != m_splitter->widget(0))
            m_splitter->replaceWidget(0, widget);
    } else
    {
        if (m_botActive)
        {
            m_splitter->insertWidget(0, widget);
        } else
            m_splitter->addWidget(widget);
    }
    m_topActive = true;
    SizeCheck();
}

void DockableSidebarContentView::SetBottomWidget(SidebarWidgetAndHeader *widget)
{
    m_bottomContents = widget;
    if (m_botActive)
    {
        if (m_topActive)
        {
            if (widget != m_splitter->widget(1))
                m_splitter->replaceWidget(1, widget);
        } else if (widget != m_splitter->widget(0))
            m_splitter->replaceWidget(0, widget);
    } else
        m_splitter->addWidget(widget);
    m_botActive = true;
    SizeCheck();
}

void DockableSidebarContentView::SizeCheck()
{
    if (m_topActive || m_botActive)
    {
        setMaximumWidth(10000);
        setMinimumWidth(350);
    } else
    {
        setMinimumWidth(0);
        setMaximumWidth(0);
    }
}

QSize DockableSidebarContentView::sizeHint() const
{
    QSize hint;
    if (m_topActive || m_botActive)
    {
        hint = QWidget::sizeHint();
        hint.setWidth(400);
    } else
    {
        hint = QWidget::sizeHint();
        hint.setWidth(0);
    }
    return hint;
}

DockableSidebar::DockableSidebar(ContextSidebarManager *context, SidebarPos pos,
                                 DockableSidebarContentView *contentView, QWidget *parent)
        : QWidget(parent), m_context(context), m_sidebarPos(pos),
          m_contentView(contentView)
{
    setParent(parent);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, getThemeColor(SidebarBackgroundColor));
    setPalette(pal);
    setAutoFillBackground(true);

    m_layout = new QVBoxLayout();
    setContentsMargins(0, 0, 0, 0);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addStretch();
    setLayout(m_layout);
    m_placeholderButton = nullptr;

    m_context->RegisterSidebar(this);
    setAcceptDrops(true);
}

void DockableSidebar::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/plain"))
        event->acceptProposedAction();
}

void DockableSidebar::dropEvent(QDropEvent *event)
{
    m_context->m_currentDragTarget->m_sidebar->RemoveType(m_context->m_currentDragTarget->m_type);
    size_t targetIdx = IdxForGlobalPos(mapToGlobal(event->position()).toPoint());
    AddType(m_context->m_currentDragTarget->m_type, targetIdx);

    if (m_context->m_currentDragTarget->isChecked())
    {
        m_context->m_leftContentView->DeactivateWidgetType(m_context->m_currentDragTarget->m_type);
        m_context->m_rightContentView->DeactivateWidgetType(m_context->m_currentDragTarget->m_type);
        m_contentView->ActivateWidgetType(m_context->m_currentDragTarget->m_type,
                                          (m_sidebarPos == TopLeft || m_sidebarPos == TopRight));
    }
    m_context->m_currentDragTarget->m_trashed = true;
    m_context->m_currentDragTarget->deleteLater();
    m_context->UpdateTypes();
}

void DockableSidebar::dragMoveEvent(QDragMoveEvent *event)
{
    size_t targetIdx = IdxForGlobalPos(mapToGlobal(event->position()).toPoint());
    DisplayDropPlaceholderForHeldButton(m_context->m_currentDragTarget, targetIdx);
}

void DockableSidebar::dragLeaveEvent(QDragLeaveEvent *event)
{
    RemovePlaceholder();
}

void DockableSidebar::AddButton(OrientablePushButton *button)
{
    bool bottom = (m_sidebarPos == BottomLeft || m_sidebarPos == BottomRight);
    if (bottom)
        m_layout->addWidget(button);
    else
        m_layout->insertWidget(m_layout->count() - 1, button);
    button->m_sidebar = this;
    m_buttons.push_back(button);
    button->m_context = m_context;
    if (parentWidget())
        parentWidget()->repaint();
    repaint();
}

void DockableSidebar::UpdateForTypes()
{
    for (auto button: m_buttons)
        layout()->removeWidget(button);
    if (m_placeholderButton)
        layout()->removeWidget(m_placeholderButton);
    delete layout();
    m_layout = new QVBoxLayout();

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addStretch();
    setLayout(m_layout);

    size_t i = 0;
    for (auto type: m_containedTypes)
    {
        auto button = new OrientablePushButton(type->name(), this);
        connect(button, &QPushButton::released, this, [this, button=button, type=type]()
        {
            if (!button->m_beingDragged)
                m_contentView->ActivateWidgetType(type, (m_sidebarPos == SidebarPos::TopLeft ||
                                                         m_sidebarPos == SidebarPos::TopRight));
        });
        button->setIcon(QIcon(QPixmap::fromImage(type->icon().inactive)));
        button->m_type = type;
        AddButton(button);
        button->m_idx = i++;
    }
    repaint();
    if (parentWidget())
        parentWidget()->repaint();
    HighlightActiveButton();
}

void DockableSidebar::ButtonMovingOut(OrientablePushButton *button)
{
    m_layout->removeWidget(button);
    m_buttons.erase(std::remove(m_buttons.begin(), m_buttons.end(), button), m_buttons.end());
}


size_t DockableSidebar::IdxForGlobalPos(QPoint _pos)
{
    size_t idx = 0;
    size_t startPos = parentWidget()->mapToGlobal(pos()).y();
    for (auto btn : m_buttons)
    {
        size_t start = startPos;
        size_t end = startPos + btn->height();
        if (idx == 0 && _pos.y() < end)
            return 0;

        if (start <= _pos.y() && _pos.y() <= end)
            return idx;

        startPos = end;
        idx++;
    }
    return idx;
}

void DockableSidebar::DisplayDropPlaceholderForHeldButton(OrientablePushButton *button, size_t idx)
{
    bool bottom = (m_sidebarPos == BottomLeft || m_sidebarPos == BottomRight);

    if (m_placeholderButton)
    {
        m_layout->removeWidget(m_placeholderButton);
        m_placeholderButton->setVisible(false);
    }
    m_placeholderButton = new QWidget();
    m_placeholderButton->setMouseTracking(false);
    m_placeholderButton->setFixedSize(button->size());
    m_placeholderButton->setStyleSheet(
            QString("QWidget {background-color: ") + getThemeColor(SidebarBackgroundColor).darker(40).name() + ";}");
    m_layout->insertWidget(bottom ? idx + 1 : idx, m_placeholderButton);
}

void DockableSidebar::RemovePlaceholder()
{
    if (m_placeholderButton)
    {
        m_placeholderButton->setVisible(false);
        m_layout->removeWidget(m_placeholderButton);
    }
}

void DockableSidebar::HighlightActiveButton()
{
    if (!m_contentView)
        return;
    for (auto button: m_buttons)
    {
        if (button->m_type == m_contentView->m_topType)
        {
            button->setChecked(true);
            button->setStyleSheet("QPushButton {border: 0px; background-color: " + getThemeColor(SidebarBackgroundColor).darker(140).name() + ";}");
            continue;
        }
        if (button->m_type == m_contentView->m_bottomType)
        {
            button->setChecked(true);
            button->setStyleSheet("QPushButton {border: 0px; background-color: " + getThemeColor(SidebarBackgroundColor).darker(140).name() + ";}");
            continue;
        }
        button->setChecked(false);
        button->setStyleSheet("QPushButton {border: 0px; background-color: " + getThemeColor(SidebarBackgroundColor).name() + ";}");
    }
}

