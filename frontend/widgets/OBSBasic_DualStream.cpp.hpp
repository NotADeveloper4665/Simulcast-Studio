/******************************************************************************
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "frontend/build/frontend-widgets.pch.hpp"

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QAction>
#include <QToolBar>
#include <QInputDialog>
#include <QAbstractItemModel>
#include <QMouseEvent>
#include <QStyle>
#include <QDropEvent>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QPainter>
#include <utility/display-helpers.hpp>
#include <functional>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <limits>

namespace {
constexpr char VerticalCanvasName[] = "Dual Stream Vertical";
constexpr char VerticalSceneName[] = "Vertical Program";
constexpr int VerticalWidth = 1080;
constexpr int VerticalHeight = 1920;

// Apply the final list order after Qt completes an internal drag, rather
// than rebuilding the list from OBS while Qt still owns the drag event.
// The row widget paints its own label, icon and buttons. Let Qt paint
// only the selection/focus background, not a second copy of the name.
class PortraitSourceDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;

	void paint(QPainter *painter, const QStyleOptionViewItem &option,
		   const QModelIndex &index) const override
	{
		QStyleOptionViewItem background(option);
		initStyleOption(&background, index);
		background.text.clear();
		background.icon = QIcon();
		background.features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration);
		const auto *widget = background.widget;
		const auto *style = widget ? widget->style() : QApplication::style();
		style->drawControl(QStyle::CE_ItemViewItem, &background, painter, widget);
	}
};

class PortraitOrderList : public QListWidget {
public:
	using QListWidget::QListWidget;
	std::function<void()> orderChanged;

protected:
	void dropEvent(QDropEvent *event) override
	{
		QListWidget::dropEvent(event);
		if (event->isAccepted() && orderChanged)
			orderChanged();
	}
};

int HardwareEncoderPriority(const char *id)
{
	if (!id)
		return -1;
	const QString encoder = QString::fromUtf8(id).toLower();
	if (encoder.contains(QStringLiteral("nvenc")))
		return 0;
	if (encoder.contains(QStringLiteral("vaapi")))
		return 1;
	if (encoder.contains(QStringLiteral("qsv")))
		return 2;
	if (encoder.contains(QStringLiteral("amf")))
		return 3;
	if (encoder.contains(QStringLiteral("videotoolbox")))
		return 4;
	return -1;
}

QString AvailableH264HardwareEncoder()
{
	QString best;
	int bestPriority = std::numeric_limits<int>::max();
	const char *id = nullptr;
	for (size_t index = 0; obs_enum_encoder_types(index, &id); ++index) {
		const char *codec = obs_get_encoder_codec(id);
		const int priority = HardwareEncoderPriority(id);
		if (obs_get_encoder_type(id) == OBS_ENCODER_VIDEO && codec && strcmp(codec, "h264") == 0 && priority >= 0 &&
		    priority < bestPriority) {
			best = QString::fromUtf8(id);
			bestPriority = priority;
		}
	}
	return best;
}

QString PreferredH264Encoder()
{
	const QString hardware = AvailableH264HardwareEncoder();
	if (!hardware.isEmpty())
		return hardware;
	QString firstAvailable;
	const char *id = nullptr;
	for (size_t index = 0; obs_enum_encoder_types(index, &id); ++index) {
		const char *codec = obs_get_encoder_codec(id);
		if (obs_get_encoder_type(id) != OBS_ENCODER_VIDEO || !codec || strcmp(codec, "h264") != 0)
			continue;
		const QString encoderId = QString::fromUtf8(id);
		if (firstAvailable.isEmpty())
			firstAvailable = encoderId;
		if (encoderId == QStringLiteral("obs_x264"))
			return encoderId;
	}
	return firstAvailable;
}

bool GetVerticalVideoInfo(obs_video_info &video)
{
	if (!obs_get_video_info(&video)) {
		return false;
	}

	video.base_width = video.output_width = VerticalWidth;
	video.base_height = video.output_height = VerticalHeight;
	return true;
}

} // namespace

obs_canvas_t *OBSBasic::GetVerticalCanvas() const
{
	for (const auto &canvas : canvases) {
		const char *name = obs_canvas_get_name(canvas);
		if (name && strcmp(name, VerticalCanvasName) == 0) {
			return canvas;
		}
	}
	return nullptr;
}

void OBSBasic::InitVerticalCanvasVideo()
{
	auto *canvas = GetVerticalCanvas();
	obs_video_info video{};
	if (!canvas || obs_canvas_has_video(canvas) || !obs_get_video_info(&video)) {
		return;
	}

	video.base_width = video.output_width = VerticalWidth;
	video.base_height = video.output_height = VerticalHeight;
	obs_canvas_reset_video(canvas, &video);
}

OBSScene OBSBasic::GetVerticalScene(bool create)
{
	auto *canvas = GetVerticalCanvas();

	if (!canvas && !create) {
		return {};
	}

	if (!canvas) {
		obs_video_info video{};
		if (!GetVerticalVideoInfo(video)) {
			return {};
		}
		OBSCanvasAutoRelease created = obs_canvas_create(VerticalCanvasName, &video, ACTIVATE | SCENE_REF);
		if (!created) {
			return {};
		}
		canvas = created;
		canvases.emplace_back(obs_canvas_get_ref(canvas));
		OnEvent(OBS_FRONTEND_EVENT_CANVAS_ADDED);
	}

	if (create) {
		InitVerticalCanvasVideo();
		if (!obs_canvas_has_video(canvas))
			return {};
	}
	OBSScene scene;
	// Resolve the selected UUID only inside the portrait canvas.
	struct Selection {
		QString uuid;
		OBSScene selected;
		OBSScene first;
	} selection{verticalSceneUuid, {}, {}};
	obs_canvas_enum_scenes(canvas, [](void *data, obs_source_t *source) {
		auto &state = *static_cast<Selection *>(data);
		if (!obs_source_is_scene(source))
			return true;
		OBSScene candidate(obs_scene_from_source(source));
		if (!state.first)
			state.first = candidate;
		if (QString::fromUtf8(obs_source_get_uuid(source)) == state.uuid) {
			state.selected = candidate;
			return false;
		}
		return true;
	}, &selection);
	scene = selection.selected ? selection.selected : selection.first;
	if (!scene && create) {
		OBSSceneAutoRelease created = obs_canvas_scene_create(canvas, VerticalSceneName);
		scene = OBSScene(created.Get());
	}
	if (scene && create) {
		verticalSceneUuid = QString::fromUtf8(obs_source_get_uuid(obs_scene_get_source(scene)));
		if (verticalPreview && verticalPreviewPanel && !verticalPreviewPanel->isHidden())
			verticalPreview->SetPortraitScene(scene);
		if (IsPreviewProgramMode()) {
			OBSSourceAutoRelease channel = obs_canvas_get_channel(canvas, 0);
			if (!verticalStudioProgramReady || !channel) {
				auto programScene = FindVerticalScene(verticalProgramUuid);
				CommitVerticalProgram(programScene ? programScene : scene);
			}
		} else {
			OBSSourceAutoRelease channel = obs_canvas_get_channel(canvas, 0);
			if (channel.Get() != obs_scene_get_source(scene))
				obs_canvas_set_channel(canvas, 0, obs_scene_get_source(scene));
			verticalProgramUuid = verticalSceneUuid;
			verticalStudioProgramReady = false;
		}
	}
	return scene;
}

OBSScene OBSBasic::FindVerticalScene(const QString &uuid) const
{
	if (uuid.isEmpty())
		return {};
	auto *canvas = GetVerticalCanvas();
	if (!canvas)
		return {};
	struct Lookup {
		QString uuid;
		OBSScene scene;
	} lookup{uuid, {}};
	obs_canvas_enum_scenes(canvas, [](void *data, obs_source_t *source) {
		auto &result = *static_cast<Lookup *>(data);
		if (obs_source_is_scene(source) && QString::fromUtf8(obs_source_get_uuid(source)) == result.uuid) {
			result.scene = OBSScene(obs_scene_from_source(source));
			return false;
		}
		return true;
	}, &lookup);
	return lookup.scene;
}

bool OBSBasic::CommitVerticalProgram(OBSScene scene)
{
	auto *canvas = GetVerticalCanvas();
	if (!canvas || !scene || !obs_canvas_has_video(canvas))
		return false;
	// Freeze scene-item layout in a private copy, as horizontal Studio Mode
	// does with scene duplication enabled. Source references remain shared.
	const auto *source = obs_scene_get_source(scene);
	OBSSceneAutoRelease snapshot = obs_scene_duplicate(scene, obs_source_get_name(source), OBS_SCENE_DUP_PRIVATE_REFS);
	if (!snapshot) {
		blog(LOG_WARNING, "Simulcast: unable to duplicate vertical program scene");
		return false;
	}
	obs_canvas_set_channel(canvas, 0, obs_scene_get_source(snapshot));
	verticalProgramUuid = QString::fromUtf8(obs_source_get_uuid(source));
	verticalStudioProgramReady = true;
	UpdateVerticalStudioUI();
	return true;
}

void OBSBasic::TransitionVertical()
{
	if (!IsPreviewProgramMode())
		return;
	auto scene = GetVerticalScene(true);
	if (scene && CommitVerticalProgram(scene))
		SaveProject();
}

void OBSBasic::SetVerticalStudioMode(bool enabled)
{
	if (!GetVerticalCanvas() && (!verticalPreviewPanel || verticalPreviewPanel->isHidden())) {
		UpdateVerticalStudioUI();
		return;
	}
	if (enabled) {
		verticalStudioProgramReady = false;
		// Keep the previously live scene on entry.
		GetVerticalScene(true);
	} else {
		// Return to the actual program scene, not an uncommitted preview.
		if (FindVerticalScene(verticalProgramUuid))
			verticalSceneUuid = verticalProgramUuid;
		if (verticalPreview)
			verticalPreview->ResetEditingState();
		verticalStudioProgramReady = false;
		GetVerticalScene(true);
		verticalLayersFingerprint.clear();
		RefreshVerticalScenes();
		RefreshVerticalLayers();
	}
	UpdateVerticalStudioUI();
}

void OBSBasic::UpdateVerticalStudioUI()
{
	if (!verticalPreviewPanel || !verticalProgramPanel)
		return;
	const bool studio = IsPreviewProgramMode();
	verticalProgramPanel->setVisible(studio);
	verticalTransitionButton->setVisible(studio);
	const auto preview = FindVerticalScene(verticalSceneUuid);
	const auto program = FindVerticalScene(verticalProgramUuid);
	const QString previewName = preview ? QString::fromUtf8(obs_source_get_name(obs_scene_get_source(preview))) : QString();
	const QString programName = program ? QString::fromUtf8(obs_source_get_name(obs_scene_get_source(program))) : QString();
	verticalPreviewTitle->setText(studio ? tr("Vertical Preview: %1").arg(previewName) : tr("Vertical Preview (9:16)"));
	verticalProgramTitle->setText(tr("Vertical Program: %1").arg(programName));
	verticalTransitionButton->setEnabled(studio && !!preview);
	ui->previewLayout->setStretchFactor(verticalPreviewPanel, studio ? 2 : 1);
}

void OBSBasic::ClearVerticalStudioState()
{
	if (verticalPreview) {
		verticalPreview->ResetEditingState();
		verticalPreview->SetPortraitScene({});
	}
	if (auto *canvas = GetVerticalCanvas())
		obs_canvas_set_channel(canvas, 0, nullptr);
	verticalStudioProgramReady = false;
	verticalProgramUuid.clear();
	verticalSceneUuid.clear();
	verticalSceneOrder.clear();
	verticalLayersFingerprint.clear();
}

void OBSBasic::RefreshVerticalScenes()
{
	if (!verticalScenes)
		return;
	QHash<QString, QString> names;
	QStringList discovered;
	if (auto *canvas = GetVerticalCanvas()) {
		using SceneList = std::pair<QHash<QString, QString> *, QStringList *>;
		SceneList data{&names, &discovered};
		obs_canvas_enum_scenes(canvas, [](void *param, obs_source_t *source) {
			if (obs_source_is_scene(source)) {
				auto &out = *static_cast<SceneList *>(param);
				const QString uuid = QString::fromUtf8(obs_source_get_uuid(source));
				out.first->insert(uuid, QString::fromUtf8(obs_source_get_name(source)));
				out.second->append(uuid);
			}
			return true;
		}, &data);
	}
	QStringList order;
	for (const auto &uuid : verticalSceneOrder)
		if (names.contains(uuid) && !order.contains(uuid))
			order.append(uuid);
	for (const auto &uuid : discovered)
		if (!order.contains(uuid))
			order.append(uuid);
	verticalSceneOrder = order;
	bool same = verticalScenes->count() == order.size();
	for (int i = 0; same && i < verticalScenes->count(); ++i)
		same = verticalScenes->item(i)->data(Qt::UserRole).toString() == order[i] &&
		       verticalScenes->item(i)->text() == names.value(order[i]);
	const QSignalBlocker block(verticalScenes);
	if (!same) {
		verticalScenes->clear();
		for (const auto &uuid : order) {
			auto *entry = new QListWidgetItem(GetSceneIcon(), names.value(uuid), verticalScenes);
			entry->setData(Qt::UserRole, uuid);
		}
	}
	const int index = order.indexOf(verticalSceneUuid);
	if (index >= 0 && verticalScenes->currentRow() != index)
		verticalScenes->setCurrentRow(index);
	UpdateVerticalStudioUI();
}

void OBSBasic::SelectVerticalScene(const QString &uuid)
{
	if (uuid.isEmpty() || uuid == verticalSceneUuid)
		return;
	if (verticalPreview)
		verticalPreview->ResetEditingState();
	if (addWindow)
		addWindow->close();
	if (transformWindow)
		transformWindow->close();
	verticalSceneUuid = uuid;
	GetVerticalScene(true);
	verticalLayersFingerprint.clear();
	RefreshVerticalScenes();
	RefreshVerticalLayers();
	SaveProject();
}

void OBSBasic::RefreshVerticalLayers()
{
	if (!verticalLayers)
		return;
	struct Row { qlonglong id; QString name; QString type; bool visible; bool locked; bool selected; };
	QList<Row> rows;
	auto scene = GetVerticalScene(false);
	if (scene) {
		obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *layer, void *data) {
			auto *list = static_cast<QList<Row> *>(data);
			auto *source = obs_sceneitem_get_source(layer);
			list->prepend(Row{static_cast<qlonglong>(obs_sceneitem_get_id(layer)),
				QString::fromUtf8(obs_source_get_name(source)), QString::fromUtf8(obs_source_get_id(source)),
				obs_sceneitem_visible(layer), obs_sceneitem_locked(layer), obs_sceneitem_selected(layer)});
			return true;
		}, &rows);
	}
	QJsonArray signature;
	for (const auto &row : rows)
		signature.append(QJsonArray{QString::number(row.id), row.name, row.type, row.visible, row.locked});
	const QString fingerprint = verticalSceneUuid + QString::fromUtf8(QJsonDocument(signature).toJson(QJsonDocument::Compact));
	const QSignalBlocker block(verticalLayers);
	if (fingerprint != verticalLayersFingerprint) {
		verticalLayersFingerprint = fingerprint;
		const auto *current = verticalLayers->currentItem();
		const auto previous = current ? current->data(Qt::UserRole).toLongLong() : -1;
		verticalLayers->clear();
		for (const auto &row : rows) {
			auto *entry = new QListWidgetItem(verticalLayers);
			entry->setData(Qt::UserRole, row.id);
			entry->setText(row.name);
			const auto icon = row.type == QStringLiteral("scene") ? GetSceneIcon() :
				row.type == QStringLiteral("group") ? GetGroupIcon() : GetSourceIcon(row.type.toUtf8().constData());
			auto *widget = new QWidget(verticalLayers);
			widget->setAttribute(Qt::WA_TranslucentBackground);
			widget->setAutoFillBackground(false);
			auto *layout = new QHBoxLayout(widget);
			layout->setContentsMargins(3, 0, 3, 0);
			layout->setSpacing(4);
			auto *iconLabel = new QLabel(widget);
			iconLabel->setPixmap(icon.pixmap(16, 16));
			iconLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
			iconLabel->setStyleSheet(QStringLiteral("background: transparent;"));
			iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
			auto *name = new QLabel(widget);
			name->setTextFormat(Qt::PlainText);
			name->setText(row.name);
			name->setToolTip(row.name);
			name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
			name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
			name->setMinimumWidth(0);
			name->setStyleSheet(QStringLiteral("background: transparent;"));
			idian::Utils::toggleClass(name, "text-muted", !row.visible);
			name->setAttribute(Qt::WA_TransparentForMouseEvents);
			layout->addWidget(iconLabel);
			layout->addWidget(name, 1);
			auto *visible = new QCheckBox(widget);
			idian::Utils::addClass(visible, "checkbox-icon");
			idian::Utils::addClass(visible, "indicator-visibility");
			visible->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
			visible->setChecked(row.visible);
			visible->setToolTip(tr("Show or hide source"));
			auto *locked = new QCheckBox(widget);
			idian::Utils::addClass(locked, "checkbox-icon");
			idian::Utils::addClass(locked, "indicator-lock");
			locked->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
			locked->setChecked(row.locked);
			locked->setToolTip(tr("Lock source"));
			layout->addWidget(visible);
			layout->addWidget(locked);
			const auto uuid = verticalSceneUuid;
			connect(visible, &QCheckBox::toggled, this, [this, uuid, id = row.id](bool checked) {
				if (uuid != verticalSceneUuid)
					return;
				auto scene = GetVerticalScene(false);
				if (scene)
					if (auto *item = obs_scene_find_sceneitem_by_id(scene, id)) {
						obs_sceneitem_set_visible(item, checked);
						SaveProject();
					}
			});
			connect(locked, &QCheckBox::toggled, this, [this, uuid, id = row.id](bool checked) {
				if (uuid != verticalSceneUuid)
					return;
				auto scene = GetVerticalScene(false);
				if (scene)
					if (auto *item = obs_scene_find_sceneitem_by_id(scene, id)) {
						obs_sceneitem_set_locked(item, checked);
						SaveProject();
					}
			});
			entry->setSizeHint(QSize(0, std::max(24, widget->sizeHint().height())));
			verticalLayers->setItemWidget(entry, widget);
			if (row.id == previous)
				verticalLayers->setCurrentItem(entry, QItemSelectionModel::NoUpdate);
		}
	}
	for (int i = 0; i < verticalLayers->count() && i < rows.size(); ++i)
		verticalLayers->item(i)->setSelected(rows[i].selected);
	const auto selected = verticalLayers->selectedItems();
	if (!selected.isEmpty() && (!verticalLayers->currentItem() || !verticalLayers->currentItem()->isSelected()))
		verticalLayers->setCurrentItem(selected.front(), QItemSelectionModel::NoUpdate);
}

void OBSBasic::AddVerticalLayer()
{
	auto scene = GetVerticalScene(true);
	if (!scene) {
		return;
	}
	if (addWindow) {
		addWindow->close();
	}
	addWindow = new OBSBasicSourceSelect(this, undo_s, scene);
	addWindow->setAttribute(Qt::WA_DeleteOnClose);
	addWindow->show();
}

void OBSBasic::EditVerticalLayer()
{
	const auto *selected = verticalLayers->currentItem();
	auto scene = GetVerticalScene(false);
	if (!scene || !selected)
		return;
	auto *item = obs_scene_find_sceneitem_by_id(scene, selected->data(Qt::UserRole).toLongLong());
	if (!item || obs_sceneitem_locked(item))
		return;
	if (transformWindow)
		transformWindow->close();
	transformWindow = new OBSBasicTransform(item, this, true);
	transformWindow->setAttribute(Qt::WA_DeleteOnClose);
	transformWindow->show();
}

QJsonArray OBSBasic::MultiStreamDestinations() const
{
	if (!Config())
		return {};
	const char *saved = config_get_string(Config(), "Stream1", "SimulcastDestinations");
	if (saved && *saved) {
		const auto document = QJsonDocument::fromJson(QByteArray(saved));
		if (document.isArray())
			return document.array();
	}
	QJsonArray entries;
	const char *configured = config_get_string(Config(), "Stream1", "DualStreamYoutubeServer");
	const QString server = configured && *configured ? QString::fromUtf8(configured)
		: QStringLiteral("rtmps://a.rtmps.youtube.com:443/live2");
	for (bool vertical : {false, true}) {
		const char *key = config_get_string(Config(), "Stream1", vertical ? "DualStreamYoutubeVerticalKey" : "DualStreamYoutubeHorizontalKey");
		if (key && *key)
			entries.append(QJsonObject{{"name", vertical ? tr("YouTube Vertical") : tr("YouTube Horizontal")},
				{"server", server}, {"key", QString::fromUtf8(key)}, {"vertical", vertical},
				{"encoder", PreferredH264Encoder()}, {"bitrate", vertical ? 6000 : 4500}});
	}
	return entries;
}

bool OBSBasic::MultiStreamEnabled() const
{
	if (!Config())
		return false;
	if (config_has_user_value(Config(), "Stream1", "SimulcastMultiStream"))
		return config_get_bool(Config(), "Stream1", "SimulcastMultiStream");
	return !MultiStreamDestinations().isEmpty();
}

bool OBSBasic::MultiStreamActive() const
{
	return std::any_of(streamDestinations.begin(), streamDestinations.end(), [](const auto &destination) {
		const auto state = destination->state.load();
		return state == StreamDestination::Connecting || state == StreamDestination::Live ||
		       state == StreamDestination::Stopping;
	});
}

bool OBSBasic::MultiStreamEditable()
{
	return !MultiStreamActive() && !streamingStarting && (!outputHandler || !outputHandler->StreamingActive());
}

void OBSBasic::SaveMultiStreamSettings(bool enabled, const QJsonArray &destinations)
{
	if (!MultiStreamEditable())
		return;
	const auto json = QJsonDocument(destinations).toJson(QJsonDocument::Compact);
	config_set_string(Config(), "Stream1", "SimulcastDestinations", json.constData());
	config_set_bool(Config(), "Stream1", "SimulcastMultiStream", enabled);
}

bool OBSBasic::EditStreamDestination(QJsonObject &entry, QWidget *parent)
{
	QDialog dialog(parent);
	dialog.setWindowTitle(entry.isEmpty() ? tr("Add Stream Destination") : tr("Edit Stream Destination"));
	auto *form = new QFormLayout(&dialog);
	auto *name = new QLineEdit(&dialog);
	auto *server = new QLineEdit(&dialog);
	name->setText(entry.value("name").toString());
	server->setText(entry.value("server").toString(QStringLiteral("rtmps://a.rtmps.youtube.com:443/live2")));
	server->setPlaceholderText(QStringLiteral("rtmps://host/app"));
	server->setToolTip(tr("For YouTube, use this same ingest URL for both destinations and enter the separate stream key shown for each orientation."));
	auto *key = new QLineEdit(&dialog);
	key->setEchoMode(QLineEdit::Password);
	key->setText(entry.value("key").toString());
	auto *orientation = new QComboBox(&dialog);
	orientation->addItems({tr("Horizontal program"), tr("Vertical program (9:16)")});
	orientation->setCurrentIndex(entry.value("vertical").toBool() ? 1 : 0);
	auto *encoders = new QComboBox(&dialog);
	const char *id = nullptr;
	for (size_t index = 0; obs_enum_encoder_types(index, &id); ++index) {
		const char *codec = obs_get_encoder_codec(id);
		if (obs_get_encoder_type(id) == OBS_ENCODER_VIDEO && codec && strcmp(codec, "h264") == 0) {
			QString label = QString::fromUtf8(obs_encoder_get_display_name(id));
			if (HardwareEncoderPriority(id) >= 0)
				label += tr(" (Hardware)");
			encoders->addItem(label, QString::fromUtf8(id));
		}
	}
	const int preferred = encoders->findData(entry.value("encoder").toString(PreferredH264Encoder()));
	if (preferred >= 0) {
		encoders->setCurrentIndex(preferred);
	}
	auto *bitrate = new QSpinBox(&dialog);
	bitrate->setRange(250, 50000);
	bitrate->setValue(entry.value("bitrate").toInt(4500));
	bitrate->setSuffix(tr(" kbps"));

	form->addRow(tr("Name"), name);
	form->addRow(tr("RTMP(S) server"), server);
	form->addRow(tr("Stream key"), key);
	form->addRow(tr("Scene"), orientation);
	form->addRow(tr("H.264 encoder"), encoders);
	form->addRow(tr("Video bitrate"), bitrate);
	auto *error = new QLabel(&dialog);
	error->setWordWrap(true);
	form->addRow(error);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	form->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
		const QUrl url(server->text().trimmed(), QUrl::StrictMode);
		if (!url.isValid() || url.host().isEmpty() ||
		    (url.scheme() != QStringLiteral("rtmp") && url.scheme() != QStringLiteral("rtmps")) ||
		    !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment() ||
		    key->text().trimmed().isEmpty() || encoders->currentIndex() < 0) {
			error->setText(tr("Enter an RTMP(S) server, a stream key and an available H.264 encoder."));
			return;
		}
		dialog.accept();
	});
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	if (dialog.exec() != QDialog::Accepted)
		return false;
	const QString title = name->text().trimmed();
	entry = QJsonObject{{"name", title.isEmpty() ? QUrl(server->text().trimmed()).host() : title},
		{"server", server->text().trimmed()}, {"key", key->text().trimmed()},
		{"vertical", orientation->currentIndex() == 1}, {"encoder", encoders->currentData().toString()},
		{"bitrate", bitrate->value()}};
	return true;
}

void OBSBasic::StartMultiStream()
{
	if (!MultiStreamEditable() || disableOutputsRef)
		return;
	const auto entries = MultiStreamDestinations();
	if (entries.isEmpty()) {
		QMessageBox::information(this, tr("Multi-Stream"), tr("Add at least one destination in Settings → Stream."));
		return;
	}
	// Dispose old inactive outputs and callbacks before replacing their owning objects.
	StopExtraStreams();
	streamDestinations.clear();
	for (const auto &value : entries) {
		const auto entry = value.toObject();
		auto destination = std::make_unique<StreamDestination>();
		destination->name = entry.value("name").toString();
		destination->server = entry.value("server").toString();
		destination->key = entry.value("key").toString();
		destination->vertical = entry.value("vertical").toBool();
		destination->encoder = entry.value("encoder").toString();
		if (destination->encoder.isEmpty())
			destination->encoder = PreferredH264Encoder();
		destination->bitrate = entry.value("bitrate").toInt(4500);
		streamDestinations.emplace_back(std::move(destination));
	}
	for (const auto &destination : streamDestinations)
		StartStreamDestination(*destination);
	if (MultiStreamActive()) {
		OnActivate();
		OnEvent(OBS_FRONTEND_EVENT_STREAMING_STARTING);
	} else {
		QMessageBox::warning(this, tr("Multi-Stream"), tr("No destinations could be started. Check the stream settings and log."));
	}
	SaveProject();
}

void OBSBasic::StopMultiStream(bool force)
{
	for (const auto &destination : streamDestinations) {
		auto state = destination->state.load();
		if ((state == StreamDestination::Connecting || state == StreamDestination::Live) &&
		    destination->state.compare_exchange_strong(state, StreamDestination::Stopping)) {
			if (force)
				obs_output_force_stop(destination->output);
			else
				obs_output_stop(destination->output);
		} else if (force && state == StreamDestination::Stopping && destination->output) {
			obs_output_force_stop(destination->output);
		}
	}
}

void OBSBasic::StartStreamDestination(StreamDestination &destination)
{
	destination.started.Disconnect();
	destination.stopped.Disconnect();
	destination.output = nullptr;
	destination.service = nullptr;

	video_t *video = obs_get_video();
	if (destination.vertical) {
		GetVerticalScene(true);
		OBSSourceAutoRelease live = GetVerticalCanvas() ? obs_canvas_get_channel(GetVerticalCanvas(), 0) : nullptr;
		OBSScene scene(obs_scene_from_source(live));
		bool hasLayer = false;
		if (scene) {
			obs_scene_enum_items(
				scene,
				[](obs_scene_t *, obs_sceneitem_t *layer, void *data) {
					auto *visible = static_cast<bool *>(data);
					*visible = obs_sceneitem_visible(layer);
					return !*visible;
				},
				&hasLayer);
		}
		if (!hasLayer) {
			destination.state.store(StreamDestination::Failed);
			blog(LOG_WARNING, "Simulcast: vertical destination needs a visible source");
			return;
		}

		video = obs_canvas_get_video(GetVerticalCanvas());
	}
	auto fail = [&] {
		destination.state.store(StreamDestination::Failed);
	};
	if (!video || !obs_get_output_flags("rtmp_output")) {
		fail();
		return;
	}

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "server", destination.server.toUtf8().constData());
	obs_data_set_string(settings, "key", destination.key.toUtf8().constData());
	destination.service = obs_service_create("rtmp_custom", "Dual Stream Service", settings, nullptr);
	destination.output = obs_output_create("rtmp_output", "Dual Stream Output", nullptr, nullptr);

	OBSDataAutoRelease videoSettings = obs_data_create();
	obs_data_set_int(videoSettings, "bitrate", destination.bitrate);
	obs_data_set_int(videoSettings, "keyint_sec", 2);
	obs_data_set_string(videoSettings, "rate_control", "CBR");
	OBSEncoderAutoRelease videoEncoder = obs_video_encoder_create(destination.encoder.toUtf8().constData(),
								      "Dual Stream H.264", videoSettings, nullptr);
	OBSDataAutoRelease audioSettings = obs_data_create();
	obs_data_set_int(audioSettings, "bitrate", 160);
	OBSEncoderAutoRelease audioEncoder =
		obs_audio_encoder_create("ffmpeg_aac", "Dual Stream AAC", audioSettings, 0, nullptr);

	if (!destination.service || !destination.output || !videoEncoder || !audioEncoder) {
		fail();
		return;
	}

	obs_encoder_set_video(videoEncoder, video);
	obs_encoder_set_audio(audioEncoder, obs_get_audio());
	obs_output_set_video_encoder(destination.output, videoEncoder);
	obs_output_set_audio_encoder(destination.output, audioEncoder, 0);
	obs_output_set_service(destination.output, destination.service);
	obs_output_set_reconnect_settings(destination.output, 10, 2);

	auto *handler = obs_output_get_signal_handler(destination.output);
	destination.started.Connect(
		handler, "start",
		[](void *data, calldata_t *) {
			auto &state = static_cast<StreamDestination *>(data)->state;
			auto connecting = StreamDestination::Connecting;
			state.compare_exchange_strong(connecting, StreamDestination::Live);
		},
		&destination);
	destination.stopped.Connect(
		handler, "stop",
		[](void *data, calldata_t *params) {
			static_cast<StreamDestination *>(data)->state.store(
				calldata_int(params, "code") == OBS_OUTPUT_SUCCESS ? StreamDestination::Idle
										   : StreamDestination::Failed);
		},
		&destination);

	destination.state.store(StreamDestination::Connecting);
	if (!obs_output_start(destination.output)) {
		fail();
	}
}

void OBSBasic::CloneSourceWithFilters(OBSScene scene, OBSSceneItem item)
{
	if (!scene || !item)
		return;
	auto *source = obs_sceneitem_get_source(item);
	if (!source || !(obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) ||
	    obs_source_is_group(source) || obs_source_is_scene(source))
		return;
	const QString suggestion = QString::fromUtf8(obs_source_get_name(source)) + tr(" Clone");
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("Clone Source"), tr("Clone name"),
		QLineEdit::Normal, suggestion, &ok).trimmed();
	if (!ok || name.isEmpty())
		return;
	OBSSourceAutoRelease existing = obs_get_source_by_name(name.toUtf8().constData());
	if (existing) {
		QMessageBox::warning(this, tr("Clone Source"), tr("That source name is already in use."));
		return;
	}
	// Duplicate the scene item, not the capture device. The public group
	// supplies a separate filter chain and is serialized by normal OBS scene saving.
	auto *copy = obs_scene_add(scene, source);
	if (!copy)
		return;
	obs_transform_info transform{};
	obs_sceneitem_crop crop{};
	obs_sceneitem_get_info2(item, &transform);
	obs_sceneitem_get_crop(item, &crop);
	obs_sceneitem_set_info2(copy, &transform);
	obs_sceneitem_set_crop(copy, &crop);
	obs_sceneitem_t *items[] = {copy};
	auto *clone = obs_scene_insert_group(scene, name.toUtf8().constData(), items, 1);
	if (!clone) {
		obs_sceneitem_remove(copy);
		return;
	}
	obs_sceneitem_select(item, false);
	obs_sceneitem_select(clone, true);
	RefreshVerticalLayers();
	SaveProject();
	CreateFiltersWindow(obs_sceneitem_get_source(clone));
}

QString OBSBasic::CleanSourceRecordingFolder() const
{
	const char *configured = Config() ? config_get_string(Config(), "Video", "CleanSourceDirectory") : nullptr;
	if (configured && *configured)
		return QString::fromUtf8(configured);
	return QDir(DualRecordingFolder(false)).filePath(QStringLiteral("Clean Sources"));
}

void OBSBasic::StopCleanSourceRecording(bool force)
{
	if (!cleanSourceRecording)
		return;
	auto &recording = *cleanSourceRecording;
	const auto state = recording.state.load();
	if (recording.output && (state == DualRecording::Recording || state == DualRecording::Stopping)) {
		if (!force) {
			if (state == DualRecording::Recording) {
				recording.state.store(DualRecording::Stopping);
				obs_output_stop(recording.output);
			}
			return;
		}
		recording.started.Disconnect();
		recording.stopped.Disconnect();
		obs_output_force_stop(recording.output);
	}
	recording.started.Disconnect();
	recording.stopped.Disconnect();
	recording.output = nullptr;
	obs_wait_for_destroy_queue();
	if (cleanSourceCanvas)
		obs_canvas_set_channel(cleanSourceCanvas, 0, nullptr);
	cleanSourceCanvas = nullptr;
	cleanSourceRecording.reset();
	if (force)
		cleanSourceSession = false;
}

void OBSBasic::UpdateCleanSourceRecording()
{
	if (isClosing() || !Config())
		return;
	const bool session = StreamingActive() || RecordingActive() || DualRecordingsActive();
	if (!session) {
		cleanSourceSession = false;
		if (cleanSourceRecording) {
			StopCleanSourceRecording();
			if (!cleanSourceRecording)
				OnDeactivate();
		}
		return;
	}
	if (cleanSourceSession || cleanSourceRecording)
		return;
	cleanSourceSession = true;
	if (!config_get_bool(Config(), "Video", "RecordCleanSource"))
		return;
	const char *uuid = config_get_string(Config(), "Video", "CleanSourceUuid");
	OBSSourceAutoRelease source = uuid && *uuid ? obs_get_source_by_uuid(uuid) : nullptr;
	obs_video_info video{};
	if (!source || obs_source_removed(source) || !(obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) ||
	    !obs_get_video_info(&video)) {
		blog(LOG_WARNING, "Simulcast: clean recording source unavailable");
		return;
	}
	// Separate ephemeral canvas: no scene items, overlays or program transitions.
	const uint32_t width = obs_source_get_width(source) & ~3u;
	const uint32_t height = obs_source_get_height(source) & ~1u;
	if (width < 4 || height < 2) {
		blog(LOG_WARNING, "Simulcast: clean recording source has no video dimensions");
		return;
	}
	video.base_width = video.output_width = width;
	video.base_height = video.output_height = height;
	cleanSourceCanvas = obs_canvas_create_private("Simulcast Clean Source", &video, DEVICE);
	if (!cleanSourceCanvas || !obs_canvas_has_video(cleanSourceCanvas)) {
		cleanSourceCanvas = nullptr;
		blog(LOG_WARNING, "Simulcast: could not create clean recording canvas");
		return;
	}
	obs_canvas_set_channel(cleanSourceCanvas, 0, source);
	cleanSourceRecording = std::make_unique<DualRecording>();
	cleanSourceRecording->cleanSource = true;
	StartDualRecording(*cleanSourceRecording);
	if (cleanSourceRecording->state.load() == DualRecording::Failed)
		StopCleanSourceRecording();
}

void OBSBasic::StartDualRecording(DualRecording &recording)
{
	const auto state = recording.state.load();
	if (state == DualRecording::Recording || state == DualRecording::Stopping)
		return;

	const QString directory = recording.cleanSource ? CleanSourceRecordingFolder() : DualRecordingFolder(recording.vertical);
	if (!QDir().mkpath(directory)) {
		recording.state.store(DualRecording::Failed);
		QMessageBox::warning(this, tr("Dual Recording"), tr("Could not create the recording folder:\n%1").arg(directory));
		return;
	}

	video_t *video = obs_get_video();
	if (recording.cleanSource) {
		video = cleanSourceCanvas ? obs_canvas_get_video(cleanSourceCanvas) : nullptr;
	} else if (recording.vertical) {
		if (!GetVerticalScene(true)) {
			QMessageBox::information(this, tr("Dual Recording"), tr("Add a source to the vertical scene before recording it."));
			return;
		}
		video = obs_canvas_get_video(GetVerticalCanvas());
	}
	if (!video || !obs_get_output_flags("ffmpeg_muxer")) {
		recording.state.store(DualRecording::Failed);
		return;
	}

	const QString prefix = recording.cleanSource ? QStringLiteral("Simulcast-Clean-Source") : recording.vertical ? QStringLiteral("Simulcast-Vertical")
						  : QStringLiteral("Simulcast-Horizontal");
	const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
	QString path;
	for (int suffix = 1; suffix < 10000; ++suffix) {
		const QString collision = suffix == 1 ? QString() : QStringLiteral("-%1").arg(suffix);
		path = QDir(directory).filePath(QStringLiteral("%1-%2%3.mkv").arg(prefix, stamp, collision));
		if (!QFileInfo::exists(path))
			break;
	}

	// Release any finished output only after disconnecting its signal handles.
	recording.started.Disconnect();
	recording.stopped.Disconnect();
	recording.output = nullptr;
	OBSDataAutoRelease outputSettings = obs_data_create();
	obs_data_set_string(outputSettings, "path", path.toUtf8().constData());
	recording.output = obs_output_create("ffmpeg_muxer", recording.vertical ? "Simulcast Vertical Recording"
								       : "Simulcast Horizontal Recording",
				     outputSettings, nullptr);
	const QString videoEncoderId = PreferredH264Encoder();
	if (videoEncoderId.isEmpty()) {
		recording.state.store(DualRecording::Failed);
		QMessageBox::warning(this, tr("Dual Recording"), tr("No H.264 encoder is available."));
		return;
	}
	OBSDataAutoRelease videoSettings = obs_data_create();
	obs_data_set_int(videoSettings, "keyint_sec", 2);
	if (HardwareEncoderPriority(videoEncoderId.toUtf8().constData()) >= 0) {
		obs_data_set_string(videoSettings, "rate_control", "CBR");
		obs_data_set_int(videoSettings, "bitrate", 16000);
	} else {
		obs_data_set_string(videoSettings, "rate_control", "CRF");
		obs_data_set_int(videoSettings, "crf", 18);
		obs_data_set_string(videoSettings, "preset", "veryfast");
	}
	OBSEncoderAutoRelease videoEncoder = obs_video_encoder_create(videoEncoderId.toUtf8().constData(),
								      "Simulcast Recording H.264", videoSettings, nullptr);
	OBSDataAutoRelease audioSettings = obs_data_create();
	obs_data_set_int(audioSettings, "bitrate", 320);
	OBSEncoderAutoRelease audioEncoder = obs_audio_encoder_create("ffmpeg_aac", "Simulcast Recording AAC",
								      audioSettings, 0, nullptr);
	if (!recording.output || !videoEncoder || !audioEncoder) {
		recording.output = nullptr;
		recording.state.store(DualRecording::Failed);
		QMessageBox::warning(this, tr("Dual Recording"), tr("Could not create H.264/AAC recording encoders."));
		return;
	}

	obs_encoder_set_video(videoEncoder, video);
	obs_encoder_set_audio(audioEncoder, obs_get_audio());
	obs_output_set_video_encoder(recording.output, videoEncoder);
	obs_output_set_audio_encoder(recording.output, audioEncoder, 0);
	recording.started.Disconnect();
	recording.stopped.Disconnect();
	auto *handler = obs_output_get_signal_handler(recording.output);
	recording.started.Connect(handler, "start", [](void *data, calldata_t *) {
		static_cast<DualRecording *>(data)->state.store(DualRecording::Recording);
	}, &recording);
	recording.stopped.Connect(handler, "stop", [](void *data, calldata_t *params) {
		static_cast<DualRecording *>(data)->state.store(
			calldata_int(params, "code") == OBS_OUTPUT_SUCCESS ? DualRecording::Idle : DualRecording::Failed);
	}, &recording);
	recording.state.store(DualRecording::Stopping);
	if (!obs_output_start(recording.output)) {
		recording.state.store(DualRecording::Failed);
		recording.started.Disconnect();
		recording.stopped.Disconnect();
		recording.output = nullptr;
		QMessageBox::warning(this, tr("Dual Recording"), tr("Could not start recording to:\n%1").arg(path));
	}
}

void OBSBasic::StopVerticalRecording(bool force)
{
	if (!verticalRecording || !verticalRecording->output)
		return;
	auto &recording = *verticalRecording;
	const auto state = recording.state.load();
	if (state != DualRecording::Recording && state != DualRecording::Stopping)
		return;
	if (verticalStopRequested && !force)
		return;
	verticalStopRequested = true;
	recording.state.store(DualRecording::Stopping);
	if (force)
		obs_output_force_stop(recording.output);
	else
		obs_output_stop(recording.output);
}

void OBSBasic::StopDualRecordings()
{
	if (!verticalRecording)
		return;
	StopVerticalRecording(true);
	auto &recording = *verticalRecording;
	recording.started.Disconnect();
	recording.stopped.Disconnect();
	recording.output = nullptr;
	obs_wait_for_destroy_queue();
	recording.state.store(DualRecording::Idle);
	verticalStopRequested = false;
}

QString OBSBasic::DualRecordingFolder(bool vertical) const
{
	const char *key = vertical ? "DualRecordVerticalDirectory" : "DualRecordHorizontalDirectory";
	const char *stored = config_get_string(App()->GetUserConfig(), "BasicWindow", key);
	if (stored && *stored)
		return QString::fromUtf8(stored);
	const QString movies = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
	const QString root = QDir(movies.isEmpty() ? QDir::home().filePath(QStringLiteral("Videos")) : movies)
				     .filePath(QStringLiteral("Simulcast"));
	return QDir(root).filePath(vertical ? QStringLiteral("Vertical") : QStringLiteral("Horizontal"));
}

bool OBSBasic::DualRecordingsActive() const
{
	if (!verticalRecording)
		return false;
	const auto state = verticalRecording->state.load();
	return state == DualRecording::Recording || state == DualRecording::Stopping;
}

bool OBSBasic::VerticalRecordingEnabled() const
{
	return Config() && config_get_bool(Config(), "Video", "RecordVertical");
}

void OBSBasic::SaveVerticalRecordingFolder(const QString &vertical)
{
	config_set_string(App()->GetUserConfig(), "BasicWindow", "DualRecordVerticalDirectory", vertical.toUtf8().constData());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

bool OBSBasic::ExtraStreamsActive() const
{
	const bool destinationsActive = std::any_of(streamDestinations.begin(), streamDestinations.end(), [](const auto &destination) {
		const auto state = destination->state.load();
		return state == StreamDestination::Connecting || state == StreamDestination::Live ||
		       state == StreamDestination::Stopping;
	});
	return destinationsActive || DualRecordingsActive() || (cleanSourceRecording &&
		(cleanSourceRecording->state.load() == DualRecording::Recording || cleanSourceRecording->state.load() == DualRecording::Stopping));
}

void OBSBasic::StopExtraStreams()
{
	multiStreamFrontendLive.store(false);
	bool hadOutputs = false;
	for (const auto &destination : streamDestinations) {
		if (!destination->output) {
			continue;
		}
		hadOutputs = true;
		destination->started.Disconnect();
		destination->stopped.Disconnect();
		const auto state = destination->state.load();
		if (state == StreamDestination::Connecting || state == StreamDestination::Live ||
		    state == StreamDestination::Stopping) {
			obs_output_force_stop(destination->output);
		}
		destination->output = nullptr;
		destination->service = nullptr;
		destination->state.store(StreamDestination::Idle);
	}
	if (hadOutputs) {
		obs_wait_for_destroy_queue();
	}
}

void OBSBasic::UpdateVerticalPreview()
{
	if (verticalPreviewPanel && !verticalPreviewPanel->isHidden()) {
		GetVerticalScene(true);
		RefreshVerticalScenes();
		RefreshVerticalLayers();
	}
}

void OBSBasic::InitDualStreamDock()
{
	verticalPreviewPanel = new QWidget(ui->canvasEditor);
	verticalPreviewPanel->setObjectName(QStringLiteral("verticalPreviewPanel"));
	auto *portraitLayout = new QVBoxLayout(verticalPreviewPanel);
	portraitLayout->setContentsMargins(0, 0, 0, 0);
	portraitLayout->setSpacing(2);
	auto *portraitViews = new QHBoxLayout();
	portraitViews->setContentsMargins(0, 0, 0, 0);
	portraitViews->setSpacing(4);
	portraitLayout->addLayout(portraitViews, 1);
	auto *previewPane = new QWidget(verticalPreviewPanel);
	auto *previewLayout = new QVBoxLayout(previewPane);
	previewLayout->setContentsMargins(0, 0, 0, 0);
	previewLayout->setSpacing(0);
	verticalPreviewTitle = new QLabel(tr("Vertical Preview (9:16)"), previewPane);
	verticalPreviewTitle->setAlignment(Qt::AlignCenter);
	verticalPreviewTitle->setProperty("class", "label-preview-title");
	previewLayout->addWidget(verticalPreviewTitle);

	verticalPreview = new OBSBasicPreview(previewPane);
	verticalPreview->setObjectName(QStringLiteral("verticalPreview"));
	verticalPreview->InitPortrait(VerticalCanvasName, QSize(VerticalWidth, VerticalHeight));
	verticalPreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	verticalPreview->setMinimumWidth(140);
	previewLayout->addWidget(verticalPreview, 1);
	portraitViews->addWidget(previewPane, 1);
	verticalProgramPanel = new QWidget(verticalPreviewPanel);
	auto *programLayout = new QVBoxLayout(verticalProgramPanel);
	programLayout->setContentsMargins(0, 0, 0, 0);
	programLayout->setSpacing(0);
	verticalProgramTitle = new QLabel(tr("Vertical Program"), verticalProgramPanel);
	verticalProgramTitle->setAlignment(Qt::AlignCenter);
	verticalProgramTitle->setProperty("class", "label-preview-title");
	programLayout->addWidget(verticalProgramTitle);
	verticalProgramDisplay = new OBSQTDisplay(verticalProgramPanel);
	verticalProgramDisplay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	verticalProgramDisplay->setMinimumWidth(100);
	connect(verticalProgramDisplay, &OBSQTDisplay::DisplayCreated, this, [](OBSQTDisplay *display) {
		obs_display_add_draw_callback(display->GetDisplay(), [](void *, uint32_t width, uint32_t height) {
			OBSCanvasAutoRelease canvas = obs_get_canvas_by_name(VerticalCanvasName);
			if (!canvas || !width || !height)
				return;
			int x = 0, y = 0;
			float scale = 1.0f;
			GetScaleAndCenterPos(VerticalWidth, VerticalHeight, static_cast<int>(width), static_cast<int>(height), x, y, scale);
			gs_viewport_push();
			gs_projection_push();
			gs_ortho(0, float(VerticalWidth), 0, float(VerticalHeight), -100, 100);
			gs_set_viewport(x, y, std::max(1, int(VerticalWidth * scale)), std::max(1, int(VerticalHeight * scale)));
			obs_render_canvas_texture_src_color_only(canvas);
			gs_load_vertexbuffer(nullptr);
			gs_projection_pop();
			gs_viewport_pop();
		}, nullptr);
	});
	programLayout->addWidget(verticalProgramDisplay, 1);
	portraitViews->addWidget(verticalProgramPanel, 1);
	verticalTransitionButton = new QPushButton(tr("Transition Vertical"), verticalPreviewPanel);
	verticalTransitionButton->setToolTip(tr("Send the vertical preview scene to the live vertical program (cut)."));
	connect(verticalTransitionButton, &QPushButton::clicked, this, &OBSBasic::TransitionVertical);
	portraitLayout->addWidget(verticalTransitionButton);
	verticalProgramPanel->hide();
	verticalTransitionButton->hide();
	ui->previewLayout->addWidget(verticalPreviewPanel, 1);
	verticalPreviewPanel->hide();

	auto *dock = new QDockWidget(tr("Dual Stream"), this);
	dock->setObjectName(QStringLiteral("dualStreamDock"));
	auto *content = new QWidget(dock);
	auto *layout = new QVBoxLayout(content);
	auto *sceneToolbar = new QToolBar(content);
	auto *sourceToolbar = new QToolBar(content);
	for (auto *toolbar : {sceneToolbar, sourceToolbar}) {
		toolbar->setIconSize(QSize(16, 16));
		toolbar->setFloatable(false);
		toolbar->setMovable(false);
		toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
	}
	auto *sceneList = new PortraitOrderList(content);
	verticalScenes = sceneList;
	verticalScenes->setObjectName(QStringLiteral("verticalScenes"));
	verticalScenes->setDragDropMode(QAbstractItemView::InternalMove);
	verticalScenes->setDefaultDropAction(Qt::MoveAction);
	verticalScenes->setSelectionMode(QAbstractItemView::SingleSelection);
	connect(verticalScenes, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
		if (item)
			SelectVerticalScene(item->data(Qt::UserRole).toString());
	});
	auto saveSceneOrder = [this] {
		verticalSceneOrder.clear();
		for (int i = 0; i < verticalScenes->count(); ++i)
			verticalSceneOrder.append(verticalScenes->item(i)->data(Qt::UserRole).toString());
		SaveProject();
	};
	sceneList->orderChanged = saveSceneOrder;
	auto toolbarAction = [](QToolBar *toolbar, QAction *original, const QString &name) {
		auto *action = toolbar->addAction(original->icon(), name);
		action->setProperty("class", original->property("class"));
		action->setToolTip(name);
		return action;
	};
	auto *addScene = toolbarAction(sceneToolbar, ui->actionAddScene, tr("Add Scene"));
	auto *removeScene = toolbarAction(sceneToolbar, ui->actionRemoveScene, tr("Remove Scene"));
	sceneToolbar->addSeparator();
	auto *sceneFilters = toolbarAction(sceneToolbar, ui->actionSceneFilters, tr("Scene Filters"));
	sceneToolbar->addSeparator();
	auto *sceneUp = toolbarAction(sceneToolbar, ui->actionSceneUp, tr("Move Scene Up"));
	auto *sceneDown = toolbarAction(sceneToolbar, ui->actionSceneDown, tr("Move Scene Down"));
	auto renameScene = [this] {
		auto scene = GetVerticalScene(false);
		if (!scene)
			return;
		auto *source = obs_scene_get_source(scene);
		bool ok = false;
		const auto name = QInputDialog::getText(this, tr("Rename Scene"), tr("Name"), QLineEdit::Normal,
			QString::fromUtf8(obs_source_get_name(source)), &ok).trimmed();
		if (!ok || name.isEmpty() || name == QString::fromUtf8(obs_source_get_name(source)))
			return;
		OBSSceneAutoRelease existing = obs_canvas_get_scene_by_name(GetVerticalCanvas(), name.toUtf8().constData());
		if (existing) {
			QMessageBox::warning(this, tr("Rename Scene"), tr("That scene name already exists."));
			return;
		}
		obs_source_set_name(source, name.toUtf8().constData());
		RefreshVerticalScenes();
		SaveProject();
	};
	connect(addScene, &QAction::triggered, this, [this] {
		if (!GetVerticalScene(true))
			return;
		bool ok = false;
		const auto name = QInputDialog::getText(this, tr("Add Scene"), tr("Name"), QLineEdit::Normal,
			tr("Vertical Scene"), &ok).trimmed();
		if (!ok || name.isEmpty())
			return;
		auto *canvas = GetVerticalCanvas();
		OBSSceneAutoRelease existing = obs_canvas_get_scene_by_name(canvas, name.toUtf8().constData());
		if (existing) {
			QMessageBox::warning(this, tr("Add Scene"), tr("That scene name already exists."));
			return;
		}
		OBSSceneAutoRelease scene = obs_canvas_scene_create(canvas, name.toUtf8().constData());
		if (scene)
			SelectVerticalScene(QString::fromUtf8(obs_source_get_uuid(obs_scene_get_source(scene))));
	});
	auto duplicateScene = [this] {
		auto scene = GetVerticalScene(false);
		if (!scene)
			return;
		bool ok = false;
		const auto original = QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene)));
		const auto name = QInputDialog::getText(this, tr("Duplicate Scene"), tr("Name"), QLineEdit::Normal,
			original + tr(" Copy"), &ok).trimmed();
		if (!ok || name.isEmpty())
			return;
		OBSSceneAutoRelease existing = obs_canvas_get_scene_by_name(GetVerticalCanvas(), name.toUtf8().constData());
		if (existing) {
			QMessageBox::warning(this, tr("Duplicate Scene"), tr("That scene name already exists."));
			return;
		}
		OBSSceneAutoRelease copy = obs_scene_duplicate(scene, name.toUtf8().constData(), OBS_SCENE_DUP_REFS);
		if (copy)
			SelectVerticalScene(QString::fromUtf8(obs_source_get_uuid(obs_scene_get_source(copy))));
	};
	connect(removeScene, &QAction::triggered, this, [this] {
		if (verticalScenes->count() <= 1)
			return;
		auto scene = GetVerticalScene(false);
		if (IsPreviewProgramMode() && verticalSceneUuid == verticalProgramUuid) {
			QMessageBox::information(this, tr("Remove Scene"), tr("Transition to another vertical program scene before removing the on-air scene."));
			return;
		}
		if (!scene || QMessageBox::question(this, tr("Remove Scene"), tr("Remove the selected vertical scene?")) != QMessageBox::Yes)
			return;
		const int row = verticalScenes->currentRow();
		const QString next = verticalScenes->item(row > 0 ? row - 1 : row + 1)->data(Qt::UserRole).toString();
		SelectVerticalScene(next);
		obs_canvas_scene_remove(scene);
		obs_source_remove(obs_scene_get_source(scene));
		RefreshVerticalScenes();
		RefreshVerticalLayers();
		SaveProject();
	});
	connect(sceneFilters, &QAction::triggered, this, [this] {
		if (auto scene = GetVerticalScene(false))
			CreateFiltersWindow(obs_scene_get_source(scene));
	});
	auto moveScene = [this, saveSceneOrder](int delta) {
		const int row = verticalScenes->currentRow();
		const int next = row + delta;
		if (row < 0 || next < 0 || next >= verticalScenes->count())
			return;
		const QSignalBlocker block(verticalScenes);
		auto *item = verticalScenes->takeItem(row);
		verticalScenes->insertItem(next, item);
		verticalScenes->setCurrentItem(item);
		saveSceneOrder();
	};
	connect(sceneUp, &QAction::triggered, this, [moveScene] { moveScene(-1); });
	connect(sceneDown, &QAction::triggered, this, [moveScene] { moveScene(1); });
	verticalScenes->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(verticalScenes, &QWidget::customContextMenuRequested, this, [this, addScene, removeScene, sceneFilters, renameScene, duplicateScene, sceneUp, sceneDown](const QPoint &pos) {
		if (auto *item = verticalScenes->itemAt(pos))
			verticalScenes->setCurrentItem(item);
		QMenu menu(verticalScenes);
		menu.addAction(addScene);
		menu.addAction(tr("Rename"), this, renameScene);
		menu.addAction(tr("Duplicate"), this, duplicateScene);
		menu.addAction(removeScene);
		menu.addAction(sceneFilters);
		menu.addAction(sceneUp);
		menu.addAction(sceneDown);
		menu.exec(verticalScenes->viewport()->mapToGlobal(pos));
	});
	connect(verticalScenes, &QListWidget::itemDoubleClicked, this, renameScene);
	layout->addWidget(new QLabel(tr("Vertical Scenes"), content));
	layout->addWidget(verticalScenes, 1);
	layout->addWidget(sceneToolbar);
	auto *sourceList = new PortraitOrderList(content);
	verticalLayers = sourceList;
	verticalLayers->setItemDelegate(new PortraitSourceDelegate(verticalLayers));
	verticalLayers->setDragDropMode(QAbstractItemView::InternalMove);
	verticalLayers->setDefaultDropAction(Qt::MoveAction);
	sourceList->orderChanged = [this] {
		auto scene = GetVerticalScene(false);
		if (!scene)
			return;
		QList<qlonglong> ids;
		for (int i = 0; i < verticalLayers->count(); ++i)
			ids.append(verticalLayers->item(i)->data(Qt::UserRole).toLongLong());
		for (int i = 0; i < ids.size(); ++i) {
			if (auto *item = obs_scene_find_sceneitem_by_id(scene, ids[i]))
				obs_sceneitem_set_order_position(item, static_cast<int>(ids.size()) - 1 - i);
		}
		verticalLayersFingerprint.clear();
		QTimer::singleShot(0, this, &OBSBasic::RefreshVerticalLayers);
		SaveProject();
	};
	verticalLayers->setObjectName(QStringLiteral("verticalSources"));
	verticalLayers->setSelectionMode(QAbstractItemView::ExtendedSelection);
	connect(verticalPreview, &OBSBasicPreview::portraitSelectionChanged, this, &OBSBasic::RefreshVerticalLayers);
	connect(verticalLayers, &QListWidget::itemSelectionChanged, this, [this] {
		auto scene = GetVerticalScene(false);
		if (!scene) {
			return;
		}
		for (int row = 0; row < verticalLayers->count(); ++row) {
			const auto *entry = verticalLayers->item(row);
			const auto id = entry->data(Qt::UserRole).toLongLong();
			if (auto *item = obs_scene_find_sceneitem_by_id(scene, id)) {
				obs_sceneitem_select(item, entry->isSelected());
			}
		}
	});

	layout->addWidget(new QLabel(tr("Vertical Sources"), content));
	layout->addWidget(verticalLayers, 2);
	layout->addWidget(sourceToolbar);
	auto *addLayer = toolbarAction(sourceToolbar, ui->actionAddSource, tr("Add Source"));
	auto *removeLayer = toolbarAction(sourceToolbar, ui->actionRemoveSource, tr("Remove Source"));
	sourceToolbar->addSeparator();
	auto *properties = toolbarAction(sourceToolbar, ui->actionSourceProperties, tr("Properties"));
	sourceToolbar->addSeparator();
	auto *raiseLayer = toolbarAction(sourceToolbar, ui->actionSourceUp, tr("Move Source Up"));
	auto *lowerLayer = toolbarAction(sourceToolbar, ui->actionSourceDown, tr("Move Source Down"));
	auto *editLayer = sourceToolbar->addAction(ui->actionEditTransform->icon(), tr("Edit Transform"));
	editLayer->setToolTip(tr("Edit Transform"));
	sourceToolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
	verticalRecording = std::make_unique<DualRecording>();
	verticalRecording->vertical = true;
	auto *recordingStatus = new QLabel(content);
	layout->addWidget(recordingStatus);
	auto *streamHealth = new QLabel(content);
	streamHealth->setTextFormat(Qt::PlainText);
	streamHealth->setWordWrap(true);
	streamHealth->setText(tr("Multi-stream health: idle"));
	layout->addWidget(streamHealth);

	connect(addLayer, &QAction::triggered, this, &OBSBasic::AddVerticalLayer);
	connect(editLayer, &QAction::triggered, this, &OBSBasic::EditVerticalLayer);
	auto openProperties = [this] {
		auto scene = GetVerticalScene(false);
		const auto *entry = verticalLayers->currentItem();
		if (!scene || !entry) {
			return;
		}
		if (auto *item = obs_scene_find_sceneitem_by_id(scene, entry->data(Qt::UserRole).toLongLong())) {
			CreatePropertiesWindow(obs_sceneitem_get_source(item));
		}
	};
	connect(properties, &QAction::triggered, this, openProperties);
	connect(verticalLayers, &QListWidget::itemDoubleClicked, this, openProperties);
	auto changeLayer = [this](auto change) {
		const auto *selected = verticalLayers->currentItem();
		auto scene = GetVerticalScene(false);
		if (!scene || !selected) {
			return;
		}
		const auto id = selected->data(Qt::UserRole).toLongLong();
		auto *layer = obs_scene_find_sceneitem_by_id(scene, id);
		if (!layer) {
			return;
		}
		change(layer);
		RefreshVerticalLayers();
		for (int row = 0; row < verticalLayers->count(); ++row) {
			if (verticalLayers->item(row)->data(Qt::UserRole).toLongLong() == id) {
				verticalLayers->setCurrentRow(row);
				break;
			}
		}
		SaveProject();
	};
	connect(raiseLayer, &QAction::triggered, this, [changeLayer] {
		changeLayer([](obs_sceneitem_t *item) { obs_sceneitem_set_order(item, OBS_ORDER_MOVE_UP); });
	});
	connect(lowerLayer, &QAction::triggered, this, [changeLayer] {
		changeLayer([](obs_sceneitem_t *item) { obs_sceneitem_set_order(item, OBS_ORDER_MOVE_DOWN); });
	});
	connect(removeLayer, &QAction::triggered, this, [this] {
		auto scene = GetVerticalScene(false);
		if (!scene || verticalLayers->selectedItems().isEmpty() ||
		    QMessageBox::question(this, tr("Remove Sources"), tr("Remove selected vertical sources?")) != QMessageBox::Yes)
			return;
		QList<qlonglong> ids;
		for (auto *entry : verticalLayers->selectedItems())
			ids.append(entry->data(Qt::UserRole).toLongLong());
		if (verticalPreview)
			verticalPreview->ResetEditingState();
		for (const auto id : ids)
			if (auto *item = obs_scene_find_sceneitem_by_id(scene, id))
				obs_sceneitem_remove(item);
		RefreshVerticalLayers();
		SaveProject();
	});
	verticalLayers->addAction(removeLayer);
	removeLayer->setShortcut(QKeySequence(Qt::Key_Delete));
	removeLayer->setShortcutContext(Qt::WidgetShortcut);
	verticalLayers->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(verticalLayers, &QWidget::customContextMenuRequested, this,
		[this, openProperties, changeLayer](const QPoint &pos) {
			auto scene = GetVerticalScene(false);
			auto *entry = verticalLayers->itemAt(pos);
			if (!scene || !entry) {
				return;
			}
			verticalLayers->setCurrentItem(entry);
			const auto id = entry->data(Qt::UserRole).toLongLong();
			auto *item = obs_scene_find_sceneitem_by_id(scene, id);
			if (!item) {
				return;
			}

			QMenu menu(verticalLayers);
			menu.addAction(tr("Clone Source (Independent Filters)"), this, [this, id] {
				auto target = GetVerticalScene(false);
				if (target)
					CloneSourceWithFilters(target, OBSSceneItem(obs_scene_find_sceneitem_by_id(target, id)));
			});
			menu.addAction(tr("Rename"), this, [this, changeLayer] {
				changeLayer([this](obs_sceneitem_t *item) {
					auto *source = obs_sceneitem_get_source(item);
					bool ok = false;
					const auto name = QInputDialog::getText(this, tr("Rename Source"), tr("Name"),
						QLineEdit::Normal, QString::fromUtf8(obs_source_get_name(source)), &ok).trimmed();
					if (!ok || name.isEmpty() || name == QString::fromUtf8(obs_source_get_name(source)))
						return;
					OBSSourceAutoRelease existing = obs_get_source_by_name(name.toUtf8().constData());
					if (!existing)
						obs_source_set_name(source, name.toUtf8().constData());
				});
			});
			menu.addAction(tr("Properties"), this, openProperties);
			menu.addAction(tr("Filters"), this, [this, changeLayer] {
				changeLayer([this](obs_sceneitem_t *item) {
					CreateFiltersWindow(obs_sceneitem_get_source(item));
				});
			});
			auto *transformMenu = menu.addMenu(tr("Transform"));
			transformMenu->addAction(tr("Edit Transform"), this, &OBSBasic::EditVerticalLayer);
			auto fit = [this, id = obs_sceneitem_get_id(item)](obs_bounds_type bounds) {
				auto target = GetVerticalScene(false);
				if (!target)
					return;
				auto *layer = obs_scene_find_sceneitem_by_id(target, id);
				if (!layer || obs_sceneitem_locked(layer))
					return;
				OBSDataAutoRelease before = obs_scene_save_transform_states(target, true);
				obs_transform_info info{};
				vec2_set(&info.scale, 1.0f, 1.0f);
				info.alignment = OBS_ALIGN_TOP | OBS_ALIGN_LEFT;
				info.bounds_alignment = OBS_ALIGN_CENTER;
				info.bounds_type = bounds;
				if (bounds != OBS_BOUNDS_NONE)
					vec2_set(&info.bounds, float(VerticalWidth), float(VerticalHeight));
				obs_sceneitem_set_info2(layer, &info);
				if (bounds == OBS_BOUNDS_NONE) {
					obs_sceneitem_crop crop{};
					obs_sceneitem_set_crop(layer, &crop);
				}
				OBSDataAutoRelease after = obs_scene_save_transform_states(target, true);
				if (before && after) {
					const std::string oldState(obs_data_get_json(before)), newState(obs_data_get_json(after));
					if (oldState != newState) {
						auto apply = [](const std::string &json) {
							obs_scene_load_transform_states(json.c_str());
							OBSBasic::Get()->SaveProject();
						};
						undo_s.add_action(tr("Vertical Transform"), apply, apply, oldState, newState);
					}
				}
				SaveProject();
			};
			transformMenu->addAction(tr("Fit to Screen"), this, [fit] { fit(OBS_BOUNDS_SCALE_INNER); });
			transformMenu->addAction(tr("Stretch to Screen"), this, [fit] { fit(OBS_BOUNDS_STRETCH); });
			transformMenu->addAction(tr("Reset Transform"), this, [fit] { fit(OBS_BOUNDS_NONE); });
			transformMenu->setEnabled(!obs_sceneitem_locked(item));
			auto *visibility = menu.addAction(tr("Visible"));
			visibility->setCheckable(true);
			visibility->setChecked(obs_sceneitem_visible(item));
			connect(visibility, &QAction::triggered, this, [changeLayer](bool visible) {
				changeLayer(
					[visible](obs_sceneitem_t *item) { obs_sceneitem_set_visible(item, visible); });
			});
			auto *locked = menu.addAction(tr("Locked"));
			locked->setCheckable(true);
			locked->setChecked(obs_sceneitem_locked(item));
			connect(locked, &QAction::triggered, this, [changeLayer](bool lock) {
				changeLayer([lock](obs_sceneitem_t *item) { obs_sceneitem_set_locked(item, lock); });
			});
			scene = nullptr;
			menu.exec(verticalLayers->viewport()->mapToGlobal(pos));
		});

	// The Docks menu uses setupDockAction(), which deliberately blocks dock signals.
	// Share the same preview synchronization with menu toggles and normal visibility changes.
	auto syncVerticalPreview = [this](bool visible) {
		verticalPreviewPanel->setVisible(visible);
		if (!visible) {
			verticalPreview->ResetEditingState();
			verticalPreview->SetPortraitScene({});
		}
		ui->previewLayout->setStretchFactor(ui->previewContainer, visible ? 2 : 0);
		ui->previewLayout->setStretchFactor(ui->previewDisabledWidget, visible ? 2 : 0);
		if (programWidget) {
			ui->previewLayout->setStretchFactor(programWidget, visible ? 2 : 0);
		}
		if (visible) {
			UpdateVerticalPreview();
			RefreshVerticalLayers();
		}
		UpdateVerticalStudioUI();
	};
	connect(dock, &QDockWidget::visibilityChanged, this, syncVerticalPreview);

	auto *timer = new QTimer(dock);
	connect(timer, &QTimer::timeout, this, [this, recordingStatus, streamHealth, healthClock = QElapsedTimer{}, healthSamples = QHash<QString, QPair<quint64, qint64>>{}, wasActive = false, wasLive = false, wasVerticalRecording = false]() mutable {
		if (verticalPreviewPanel && verticalPreviewPanel->isVisible() && !(QApplication::mouseButtons() & Qt::LeftButton)) {
			RefreshVerticalScenes();
			RefreshVerticalLayers();
		}
		if (!healthClock.isValid())
			healthClock.start();
		const qint64 now = healthClock.elapsed();
		QStringList healthRows;
		QHash<QString, QPair<quint64, qint64>> nextSamples;
		int healthIndex = 0;
		for (const auto &destination : streamDestinations) {
			const QString sampleKey = QString::number(healthIndex++);
			QString status;
			switch (destination->state.load()) {
			case StreamDestination::Connecting: status = tr("Connecting"); break;
			case StreamDestination::Live: status = tr("Live"); break;
			case StreamDestination::Stopping: status = tr("Stopping"); break;
			case StreamDestination::Failed: status = tr("Failed"); break;
			default: status = tr("Idle"); break;
			}
			QString row = tr("%1 (%2): %3").arg(destination->name,
				destination->vertical ? tr("Vertical") : tr("Horizontal"), status);
			if (destination->output && obs_output_active(destination->output)) {
				const quint64 bytes = obs_output_get_total_bytes(destination->output);
				const auto previous = healthSamples.constFind(sampleKey);
				if (previous != healthSamples.cend() && bytes >= previous->first && now > previous->second) {
					const double kbps = double(bytes - previous->first) * 8.0 / double(now - previous->second);
					row += tr(" · Actual %1 kbps").arg(kbps, 0, 'f', 0);
				}
				nextSamples.insert(sampleKey, qMakePair(bytes, now));
				row += tr(" · Congestion %1%").arg(double(obs_output_get_congestion(destination->output)) * 100.0, 0, 'f', 0);
				if (obs_output_reconnecting(destination->output))
					row += tr(" · Reconnecting");
				row += tr(" · Target %1 kbps · Dropped %2/%3")
					.arg(destination->bitrate)
					.arg(obs_output_get_frames_dropped(destination->output))
					.arg(obs_output_get_total_frames(destination->output));
			}
			healthRows.append(row);
		}
		healthSamples.swap(nextSamples);
		const QString healthText = healthRows.isEmpty() ? tr("Multi-stream health: idle") : healthRows.join(QLatin1Char('\n'));
		if (streamHealth->text() != healthText)
			streamHealth->setText(healthText);
		const bool active = MultiStreamActive();
		const bool live = std::any_of(streamDestinations.begin(), streamDestinations.end(), [](const auto &destination) {
			return destination->state.load() == StreamDestination::Live;
		});
		multiStreamFrontendLive.store(active && (live || wasLive));
		if (live && !wasLive)
			OnEvent(OBS_FRONTEND_EVENT_STREAMING_STARTED);
		if (wasActive && !active) {
			OnEvent(OBS_FRONTEND_EVENT_STREAMING_STOPPED);
			OnDeactivate();
		}
		if ((active || wasActive) && sysTrayStream)
			sysTrayStream->setText(active ? QTStr("Basic.Main.StopStreaming") : QTStr("Basic.Main.StartStreaming"));
		wasActive = active;
		wasLive = active && (live || wasLive);
		const bool verticalActive = DualRecordingsActive();
		if (wasVerticalRecording && !verticalActive) {
			verticalRecording->started.Disconnect();
			verticalRecording->stopped.Disconnect();
			verticalRecording->output = nullptr;
			verticalStopRequested = false;
			OnDeactivate();
		}
		wasVerticalRecording = verticalActive;
		UpdateCleanSourceRecording();
		auto stateName = [](DualRecording::State state) {
			switch (state) {
			case DualRecording::Recording: return QStringLiteral("Recording");
			case DualRecording::Stopping: return QStringLiteral("Stopping");
			case DualRecording::Failed: return QStringLiteral("Failed");
			default: return QStringLiteral("Ready");
			}
		};
		recordingStatus->setText(tr("Horizontal: %1 · Vertical: %2")
			.arg(RecordingActive() ? tr("Recording") : tr("Ready"),
				VerticalRecordingEnabled() || verticalActive ? stateName(verticalRecording->state.load()) : tr("Disabled")) +
			(cleanSourceRecording ? tr(" · Clean source: %1").arg(stateName(cleanSourceRecording->state.load())) : QString()));
	});
	timer->start(250);
	verticalPreview->setFocusPolicy(Qt::StrongFocus);
	auto nudge = [this](int key, int dx, int dy) {
		auto *action = new QAction(verticalPreview);
		action->setShortcut(QKeySequence(key));
		action->setShortcutContext(Qt::WidgetShortcut);
		verticalPreview->addAction(action);
		connect(action, &QAction::triggered, this, [this, dx, dy] { NudgeVertical(dx, dy); });
	};
	nudge(Qt::Key_Left, -1, 0);
	nudge(Qt::Key_Right, 1, 0);
	nudge(Qt::Key_Up, 0, -1);
	nudge(Qt::Key_Down, 0, 1);
	nudge(Qt::SHIFT | Qt::Key_Left, -10, 0);
	nudge(Qt::SHIFT | Qt::Key_Right, 10, 0);
	nudge(Qt::SHIFT | Qt::Key_Up, 0, -10);
	nudge(Qt::SHIFT | Qt::Key_Down, 0, 10);
	verticalPreview->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(verticalPreview, &QWidget::customContextMenuRequested, this, [this](const QPoint &) {
		RefreshVerticalLayers();
		if (auto *item = verticalLayers->currentItem())
			emit verticalLayers->customContextMenuRequested(verticalLayers->visualItemRect(item).center());
	});
	dock->setWidget(content);
	AddDockWidget(dock, Qt::RightDockWidgetArea);
	// Register after AddDockWidget: setupDockAction disconnects existing triggered slots.
	// Its visibility update runs first, even when visibilityChanged is blocked.
	connect(dock->toggleViewAction(), &QAction::triggered, this, [dock, syncVerticalPreview](bool) {
		syncVerticalPreview(!dock->isHidden());
	});
	dock->hide();
	syncVerticalPreview(false);
}
