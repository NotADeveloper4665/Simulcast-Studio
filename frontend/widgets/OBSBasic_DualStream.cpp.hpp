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

namespace {
constexpr char VerticalCanvasName[] = "Dual Stream Vertical";
constexpr char VerticalSceneName[] = "Vertical Program";
constexpr int VerticalWidth = 1080;
constexpr int VerticalHeight = 1920;

bool GetVerticalVideoInfo(obs_video_info &video)
{
	if (!obs_get_video_info(&video)) {
		return false;
	}

	video.base_width = video.output_width = VerticalWidth;
	video.base_height = video.output_height = VerticalHeight;
	return true;
}

QDialogButtonBox *DualStreamDialogButtons(QDialog &dialog)
{
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	return buttons;
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

	OBSSceneAutoRelease scene = obs_canvas_get_scene_by_name(canvas, VerticalSceneName);
	if (create) {
		InitVerticalCanvasVideo();
		if (!obs_canvas_has_video(canvas)) {
			return {};
		}
	}

	if (create && !scene) {
		scene = obs_canvas_scene_create(canvas, VerticalSceneName);
	}

	if (create && scene) {
		obs_canvas_set_channel(canvas, 0, obs_scene_get_source(scene));
	}

	return OBSScene(scene.Get());
}

void OBSBasic::RefreshVerticalLayers()
{
	const QSignalBlocker block(verticalLayers);
	verticalLayers->clear();
	auto scene = GetVerticalScene(false);
	if (!scene) {
		return;
	}

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *layer, void *data) {
			auto *list = static_cast<QListWidget *>(data);
			auto *item = new QListWidgetItem(
				QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(layer))));
			item->setData(Qt::UserRole,
				      QVariant::fromValue(static_cast<qlonglong>(obs_sceneitem_get_id(layer))));
			list->insertItem(0, item);
			item->setSelected(obs_sceneitem_selected(layer));
			return true;
		},
		verticalLayers);
	const auto selected = verticalLayers->selectedItems();
	if (!selected.isEmpty()) {
		verticalLayers->setCurrentItem(selected.front(), QItemSelectionModel::NoUpdate);
	}
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
	auto *selected = verticalLayers->currentItem();
	auto scene = GetVerticalScene(false);
	if (!scene || !selected) {
		return;
	}

	const auto id = selected->data(Qt::UserRole).toLongLong();
	auto *layer = obs_scene_find_sceneitem_by_id(scene, id);
	if (!layer) {
		return;
	}

	obs_transform_info transform{};
	obs_sceneitem_get_info2(layer, &transform);

	QDialog dialog(this);
	dialog.setWindowTitle(tr("Vertical Source Layout"));
	auto *form = new QFormLayout(&dialog);
	auto number = [&](const QString &label, float value, int minimum, int maximum) {
		auto *field = new QSpinBox(&dialog);
		field->setRange(minimum, maximum);
		field->setValue(static_cast<int>(value));
		form->addRow(label, field);
		return field;
	};
	auto *x = number(tr("X"), transform.pos.x, -4096, 4096);
	auto *y = number(tr("Y"), transform.pos.y, -4096, 4096);
	auto *width = number(tr("Width"), transform.bounds.x, 1, 4096);
	auto *height = number(tr("Height"), transform.bounds.y, 1, 4096);
	auto *fill = new QCheckBox(tr("Fill and crop to the bounds"), &dialog);
	fill->setChecked(transform.crop_to_bounds);
	form->addRow(fill);
	auto *visible = new QCheckBox(tr("Visible"), &dialog);
	visible->setChecked(obs_sceneitem_visible(layer));
	const QString sceneId = QString::fromUtf8(obs_source_get_uuid(obs_scene_get_source(scene)));
	scene = nullptr;
	form->addRow(visible);
	form->addRow(DualStreamDialogButtons(dialog));

	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	auto current = GetVerticalScene(false);
	if (current.Get() != scene.Get()) {
		return;
	}
	layer = obs_scene_find_sceneitem_by_id(scene, id);
	if (!layer) {
		return;
	}
	vec2_set(&transform.pos, static_cast<float>(x->value()), static_cast<float>(y->value()));
	vec2_set(&transform.bounds, static_cast<float>(width->value()), static_cast<float>(height->value()));
	transform.bounds_type = fill->isChecked() ? OBS_BOUNDS_SCALE_OUTER : OBS_BOUNDS_SCALE_INNER;
	transform.crop_to_bounds = fill->isChecked();
	obs_sceneitem_set_info2(layer, &transform);
	obs_sceneitem_set_visible(layer, visible->isChecked());
	SaveProject();
}

void OBSBasic::AddStreamDestination(QListWidget *list)
{
	QDialog dialog(this);
	dialog.setWindowTitle(tr("Add Stream Destination"));
	auto *form = new QFormLayout(&dialog);
	auto *name = new QLineEdit(&dialog);
	auto *server = new QLineEdit(&dialog);
	server->setPlaceholderText(QStringLiteral("rtmps://host/app"));
	auto *key = new QLineEdit(&dialog);
	key->setEchoMode(QLineEdit::Password);
	auto *orientation = new QComboBox(&dialog);
	orientation->addItems({tr("Horizontal program"), tr("Vertical program (9:16)")});
	auto *encoders = new QComboBox(&dialog);
	const char *id = nullptr;
	for (size_t index = 0; obs_enum_encoder_types(index, &id); ++index) {
		const char *codec = obs_get_encoder_codec(id);
		if (obs_get_encoder_type(id) == OBS_ENCODER_VIDEO && codec && strcmp(codec, "h264") == 0) {
			encoders->addItem(QString::fromUtf8(obs_encoder_get_display_name(id)), QString::fromUtf8(id));
		}
	}
	const int x264 = encoders->findData(QStringLiteral("obs_x264"));
	if (x264 >= 0) {
		encoders->setCurrentIndex(x264);
	}
	auto *bitrate = new QSpinBox(&dialog);
	bitrate->setRange(250, 50000);
	bitrate->setValue(4500);
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
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	auto destination = std::make_unique<StreamDestination>();
	destination->vertical = orientation->currentIndex() == 1;
	destination->name = name->text().trimmed();
	if (destination->name.isEmpty()) {
		destination->name = QUrl(server->text().trimmed()).host();
	}
	destination->server = server->text().trimmed();
	destination->key = key->text().trimmed();
	destination->encoder = encoders->currentData().toString();
	destination->bitrate = bitrate->value();
	const QString title = destination->name;
	streamDestinations.emplace_back(std::move(destination));
	list->addItem(title);
	list->setCurrentRow(list->count() - 1);
}

void OBSBasic::StartStreamDestination(StreamDestination &destination)
{
	destination.started.Disconnect();
	destination.stopped.Disconnect();
	destination.output = nullptr;
	destination.service = nullptr;

	video_t *video = obs_get_video();
	if (destination.vertical) {
		auto scene = GetVerticalScene(true);
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
			QMessageBox::information(this, tr("Multi-Stream"),
						 tr("Add a source to the vertical scene first."));
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

bool OBSBasic::ExtraStreamsActive() const
{
	return std::any_of(streamDestinations.begin(), streamDestinations.end(), [](const auto &destination) {
		const auto state = destination->state.load();
		return state == StreamDestination::Connecting || state == StreamDestination::Live ||
		       state == StreamDestination::Stopping;
	});
}

void OBSBasic::StopExtraStreams()
{
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
	}
}

void OBSBasic::InitDualStreamDock()
{
	verticalPreviewPanel = new QWidget(ui->canvasEditor);
	verticalPreviewPanel->setObjectName(QStringLiteral("verticalPreviewPanel"));
	auto *previewLayout = new QVBoxLayout(verticalPreviewPanel);
	previewLayout->setContentsMargins(0, 0, 0, 0);
	previewLayout->setSpacing(0);
	auto *title = new QLabel(tr("Vertical Preview (9:16)"), verticalPreviewPanel);
	title->setAlignment(Qt::AlignCenter);
	previewLayout->addWidget(title);

	verticalPreview = new OBSBasicPreview(verticalPreviewPanel);
	verticalPreview->setObjectName(QStringLiteral("verticalPreview"));
	verticalPreview->InitPortrait(VerticalCanvasName, QSize(VerticalWidth, VerticalHeight));
	verticalPreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	verticalPreview->setMinimumWidth(140);
	previewLayout->addWidget(verticalPreview, 1);
	ui->previewLayout->addWidget(verticalPreviewPanel, 1);
	verticalPreviewPanel->hide();

	auto *dock = new QDockWidget(tr("Dual Stream"), this);
	dock->setObjectName(QStringLiteral("dualStreamDock"));
	auto *content = new QWidget(dock);
	auto *layout = new QVBoxLayout(content);
	auto *layerButtons = new QHBoxLayout();
	auto *destinationButtons = new QHBoxLayout();
	auto *layerOrderButtons = new QHBoxLayout();
	auto *targets = new QListWidget(content);
	verticalLayers = new QListWidget(content);
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

	auto addButton = [content](QHBoxLayout *row, const QString &title) {
		auto *button = new QPushButton(title, content);
		row->addWidget(button);
		return button;
	};

	layout->addWidget(new QLabel(tr("Vertical Scene · 1080 × 1920"), content));
	layout->addWidget(verticalLayers);
	layout->addLayout(layerButtons);
	layout->addLayout(layerOrderButtons);
	auto *addLayer = addButton(layerButtons, tr("Add Source"));
	auto *editLayer = addButton(layerButtons, tr("Layout"));
	auto *properties = addButton(layerButtons, tr("Properties"));
	auto *raiseLayer = addButton(layerOrderButtons, tr("Raise"));
	auto *lowerLayer = addButton(layerOrderButtons, tr("Lower"));
	auto *removeLayer = addButton(layerOrderButtons, tr("Remove Source"));

	layout->addWidget(new QLabel(tr("Stream Destinations"), content));
	layout->addWidget(targets);
	layout->addLayout(destinationButtons);
	auto *addTarget = addButton(destinationButtons, tr("Add"));
	auto *removeTarget = addButton(destinationButtons, tr("Remove"));
	auto *startTarget = addButton(destinationButtons, tr("Start"));
	auto *stopTarget = addButton(destinationButtons, tr("Stop"));

	connect(addLayer, &QPushButton::clicked, this, &OBSBasic::AddVerticalLayer);
	connect(editLayer, &QPushButton::clicked, this, &OBSBasic::EditVerticalLayer);
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
	connect(properties, &QPushButton::clicked, this, openProperties);
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
	connect(raiseLayer, &QPushButton::clicked, this, [changeLayer] {
		changeLayer([](obs_sceneitem_t *item) { obs_sceneitem_set_order(item, OBS_ORDER_MOVE_UP); });
	});
	connect(lowerLayer, &QPushButton::clicked, this, [changeLayer] {
		changeLayer([](obs_sceneitem_t *item) { obs_sceneitem_set_order(item, OBS_ORDER_MOVE_DOWN); });
	});
	connect(removeLayer, &QPushButton::clicked, this,
		[changeLayer] { changeLayer([](obs_sceneitem_t *item) { obs_sceneitem_remove(item); }); });
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
			menu.addAction(tr("Properties"), this, openProperties);
			menu.addAction(tr("Filters"), this, [this, changeLayer] {
				changeLayer([this](obs_sceneitem_t *item) {
					CreateFiltersWindow(obs_sceneitem_get_source(item));
				});
			});
			menu.addAction(tr("Layout"), this, &OBSBasic::EditVerticalLayer);
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

	connect(dock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
		verticalPreviewPanel->setVisible(visible);
		if (!visible) {
			verticalPreview->ResetEditingState();
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
	});

	auto selectedDestination = [this, targets]() -> StreamDestination * {
		const int row = targets->currentRow();
		if (row < 0 || static_cast<size_t>(row) >= streamDestinations.size()) {
			return nullptr;
		}
		return streamDestinations[static_cast<size_t>(row)].get();
	};
	auto canStart = [](StreamDestination *destination) {
		if (!destination) {
			return false;
		}
		const auto state = destination->state.load();
		return state == StreamDestination::Idle || state == StreamDestination::Failed;
	};
	connect(addTarget, &QPushButton::clicked, this, [this, targets] { AddStreamDestination(targets); });
	connect(removeTarget, &QPushButton::clicked, this, [this, targets, selectedDestination, canStart] {
		if (!canStart(selectedDestination())) {
			return;
		}
		const int row = targets->currentRow();
		streamDestinations.erase(streamDestinations.begin() + row);
		delete targets->takeItem(row);
	});
	connect(startTarget, &QPushButton::clicked, this, [this, selectedDestination, canStart] {
		auto *destination = selectedDestination();
		if (canStart(destination)) {
			StartStreamDestination(*destination);
		}
	});
	connect(stopTarget, &QPushButton::clicked, this, [selectedDestination] {
		auto *destination = selectedDestination();
		if (!destination) {
			return;
		}
		auto state = destination->state.load();
		if ((state == StreamDestination::Connecting || state == StreamDestination::Live) &&
		    destination->state.compare_exchange_strong(state, StreamDestination::Stopping)) {
			obs_output_force_stop(destination->output);
		}
	});

	auto updateDestinations = [this, targets, selectedDestination, canStart, startTarget, stopTarget,
				   removeTarget] {
		for (int row = 0; row < targets->count(); ++row) {
			const auto &destination = *streamDestinations[static_cast<size_t>(row)];
			QString status;
			switch (destination.state.load()) {
			case StreamDestination::Idle:
				status = tr("Ready");
				break;
			case StreamDestination::Connecting:
				status = tr("Connecting");
				break;
			case StreamDestination::Live:
				status = obs_output_reconnecting(destination.output) ? tr("Reconnecting") : tr("Live");
				break;
			case StreamDestination::Stopping:
				status = tr("Stopping");
				break;
			case StreamDestination::Failed:
				status = tr("Failed. See the OBS log.");
				break;
			}
			const QString title = tr("%1 · %2 · %3")
						      .arg(destination.name,
							   destination.vertical ? tr("Vertical") : tr("Horizontal"),
							   status);
			if (targets->item(row)->text() != title) {
				targets->item(row)->setText(title);
			}
		}
		auto *destination = selectedDestination();
		startTarget->setEnabled(canStart(destination));
		removeTarget->setEnabled(canStart(destination));
		stopTarget->setEnabled(destination && !canStart(destination) &&
				       destination->state.load() != StreamDestination::Stopping);
	};

	auto *timer = new QTimer(dock);
	connect(timer, &QTimer::timeout, this, updateDestinations);
	connect(targets, &QListWidget::currentRowChanged, this, updateDestinations);
	updateDestinations();
	timer->start(250);

	dock->setWidget(content);
	AddDockWidget(dock, Qt::RightDockWidgetArea);
	dock->hide();
}
